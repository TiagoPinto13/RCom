
#include "link_layer_aux.h"
#include "serial_port.h"
#include "alarm.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>




int updateStateMachine(unsigned char byte, StateLinkL *state, LinkLayerRole role) {
    switch (*state)
    {
        case START:
            if (byte == F)
            {
                printf("passei para flag");
                *state = FLAG_RCV;
            }
            else printf("weird byte start\n");
            break;

        case FLAG_RCV:
            if ((byte == A_RX && role == LlTx) || (role == LlRx && byte== A))
            {
                *state = A_RCV;
                printf("passei para a_rcv");

            }
            else if (byte == A_RX && role == 0) {
                *state = A_RCV;
            }
            else if (byte != F) *state = START;

            else printf("weird byte flag\n");
            break;

        case A_RCV:
            if ((byte == C_Set && role == LlRx) || (byte == C_UA && role == LlTx) || (byte == C_DISC && role == 0)) {
                *state = C_RCV;
                c_byte = byte;
                printf("passei para c_rcv");
            }
            
            else if (byte == F) *state= FLAG_RCV;

            else *state = START;

            break;


        case C_RCV:
            if (((byte ==(A_RX^c_byte)) && role == LlTx) || ((byte ==(A^c_byte)) && role == LlRx)) {
                *state = BCC1_OK;
                printf("passei para bcc1_ok");
            }
            else if (byte == (A_RX^C_DISC) && role == 0) {
                *state = BCC1_OK;
            }
            
            else if (byte == F) *state= FLAG_RCV;
            else *state = START;

            break;
            

        case BCC1_OK:
            if (byte == F) {
                *state = STOP;
            }
            else {
                *state = START;
                printf("weird byte bcc1\n");
            }
            break;
        
        default:
            break;
    }
    return 0;
}

int updateStateMachineWrite() {
    unsigned char byte= 0;
    unsigned char cField = 0;
    StateLinkL state = START;
    while (state != STOP && alarmEnabled == FALSE) {  
        int r = readByteSerialPort(&byte); 
        if (r > 0 ) {
            
            switch (state)
            {
                case START:
                    printf("START\n");
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    }
                    break;
                
                case FLAG_RCV:
                    printf("FLAG_RCV\n");
                    if (byte == A_RX) state = A_RCV;
                    else  if (byte != FLAG) state = START;
                    break;

                case A_RCV:
                    printf("A_RCV\n");
                    if(byte == C_REJ[0] || byte == C_REJ[1] || byte == C_RR[0] || byte == C_RR[1] || byte == C_DISC) {
                        printf("C_RCV a entrar\n");
                        state = C_RCV;
                        cField = byte;

                    }
                    else if (byte == FLAG) {
                        state = FLAG;
                    }
                    else state = START;
                    break;

                case C_RCV:
                    printf("C_RCV\n");
                    printf("byte: %x\n", byte);
                    printf("A_RX^cField: %x\n", A_RX^cField);

                    if (byte == (A_RX ^ cField)) {
                        state = BCC1_OK;
                        printf("BCC1_OK a entrar\n");
                    } 
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;

                case BCC1_OK:
                    printf("BCC1_OK\n");
                    if (byte == FLAG){
                        state = STOP;
                        printf("STOP\n");

                    }
                    else state = START;
                    break;
                default:
                    break;
            }
        }
        else if (r == -1) {
            // Attempt to reopen the serial port
            fd = openSerialPort(serialPort, baudRate);
            if (fd != -1) {
                printf("Successfully reopened serial port\n");
                state = START;  
                return 0;       
            } else {
                return -1; 
            }
        }
    }

    return cField;

}

int updateStateMachineRead(unsigned char *packet) {
    unsigned char byte, cField = 0;
    StateLinkL state = START;
    int index = 0;
    while (state != STOP) {  
        int r = readByteSerialPort(&byte);
        if (r > 0 ) {
            switch (state)
            {
                case START:{
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    }
                    break;
                }
                
                case FLAG_RCV:{
                    if (byte == A) state = A_RCV;
                    else  if (byte != FLAG) state = START;
                    break;
                }

                case A_RCV:{
                    if(byte == C_FN[0] || byte == C_FN[1]) {

                        state = C_RCV;
                        cField = byte;
                    }
                    else if (byte == FLAG) {
                        state = FLAG;
                    }
                    else if (byte == C_DISC) {
                        
                        unsigned char supervisionFrames[BUF_SIZE] = {F, A_RX, C_DISC, A_RX^C_DISC, F};
                        if(writeBytesSerialPort(supervisionFrames, 5)<0) {
                            return -1;
                        }
                        return 0;
                    }
                    else state = START;
                    break;
                }
                case C_RCV:{
                    if (byte == (A ^ cField)) state = READING;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                }

                case READING:{
                    if (index > BUF_SIZE -1){
                        printf("Error: packet buffer overflow\n");
                        return -1; 
                    }
                    else if (byte == ESC){
                        state = FOUND_ESC;
                    }
                    else if (byte == FLAG) {
                        unsigned char bcc2 = packet[index - 1];
                        
                        index--;
                        packet[index] = '\0';
                        unsigned char acc = 0;

                        for (unsigned int j = 0; j < index; j++) {
                            acc ^= packet[j];
                        }
                    
                        
                        
                        
                        printf("acc = %x\n", acc);
                        printf("bcc = %x\n", bcc2);

                        if (bcc2 == acc){
                            state = STOP;
                            unsigned char supervisionFrames[BUF_SIZE] = {F, A_RX, C_RR[currentTramaTx], A_RX^C_RR[currentTramaTx], F};
                            if(writeBytesSerialPort(supervisionFrames, 5)<0) {
                                return -1;
                            }
                            currentTramaTx = (currentTramaTx + 1)%2;
                            printf("Packet: \n");
                            for (int i = 0; i < index; i++) {
                                printf("%02X ", packet[i]); // Imprime cada byte em hexadecimal
                            }
                            return index; 
                        }
                        else{
                            printf("Error: retransmition\n");
                            unsigned char supervisionFrames[BUF_SIZE] = {F, A_RX, C_REJ[currentTramaTx], A^C_REJ[currentTramaTx], F};
                            writeBytesSerialPort(supervisionFrames, 5);
                            return -1;
                        }
                    }
                    else {
                        packet[index++] = byte;
                    }
                    break;
                }
                
                case FOUND_ESC:{
                    state = READING;
                    if (byte == 0x5D) {
                        packet[index++] = ESC;  
                    } 
                    else if (byte == 0x5E) {
                        packet[index++] = FLAG;  
                    } 
                    else {
                        packet[index++] = ESC;
                        packet[index++] = byte;
                    }
                    break;
                }
                default:
                    break;
            }
        } 
        else if (r == -1) {
            // Attempt to reopen the serial port
            fd = openSerialPort(serialPort, baudRate);
            if (fd != -1) {
                printf("Successfully reopened serial port fd: %d\n", fd);
                state = START;  
                continue;    
            } else {
                return -1;  
            }
        }
    }
    return -1;
}