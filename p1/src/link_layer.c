// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "alarm.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>




// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 1024
#define WRITE_SIZE 5
#define ALARM_SECONDS 3
#define FLAG 0x7E
#define ESC 0x7D
#define F 0x7E
#define A 0x03
#define A_RX 0x01
// #define C_RV 0x00
// #define BCC1 A^C_RV
// #define C_UA 0x07
// #define BCC1_UA A^C_UA

#define C_Set 0x03
#define C_UA 0x07
#define C_RR ((uint8_t[]){0xAA, 0xAB})
#define C_REJ ((uint8_t[]){0x54, 0x55})
#define C_FN ((uint8_t[]){0x00, 0x80})
#define C_DISC 0x0B
unsigned char c_byte;
int retransmissions = 0;
int timeout;
unsigned char currentTramaTx = 0;
extern int alarmEnabled;
extern fd;
char serialPort[50]; 
int baudRate;
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }
    retransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    strcpy(serialPort,connectionParameters.serialPort);
    baudRate = connectionParameters.baudRate;
    StateLinkL state = START;
    if (connectionParameters.role == LlTx && state==START ) {
        unsigned char byte;
        struct sigaction act = {0};
        act.sa_handler=&alarmHandler;
        (void)sigaction(SIGALRM, &act,NULL);
        while (connectionParameters.nRetransmissions!=0 && state != STOP) {

            
            unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_Set, A^C_Set, F};
            if(writeBytesSerialPort(supervisionFrames, 5)<0) {
                return -1;
            }
            
            alarm(connectionParameters.timeout);
            alarmEnabled = FALSE;        

            while (alarmEnabled == FALSE && state!=STOP)
            {
                if (readByteSerialPort(&byte) >0) {
                    updateStateMachine(byte, &state, LlTx);

                }
                
            }
            connectionParameters.nRetransmissions--;
        }
        if (state != STOP) 
            return -1;
    }


    //parte para a pessoa que recebe

    else if (connectionParameters.role==LlRx){
        unsigned char byte;
        while (state != STOP)
        {   
            int rByte = readByteSerialPort(&byte);
            if (rByte<0)
                return -1;
            else if (rByte>0)
            {
                
                updateStateMachine(byte, &state, LlRx);
            }
            
        }
        
        unsigned char supervisionFrames[BUF_SIZE] = {F, A_RX, C_UA, A^C_UA, F};
        writeBytesSerialPort(supervisionFrames, 5);
    }
    else
        return -1;
    return 0;

}



////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int frameSize = 6+bufSize;  // (F,A,C,BCC1, bufdata, BCC2, F)
    unsigned char *dataFrame = (unsigned char *) malloc(frameSize);
    unsigned char BCC2 = 0;
    for (unsigned int i = 0; i<bufSize; i++) {
        BCC2^=buf[i];
    }
    dataFrame[0]=FLAG;
    dataFrame[1]=A;
    dataFrame[2]=C_FN[currentTramaTx];
    dataFrame[3]=dataFrame[1]^dataFrame[2];
    
    int j = 4;
    for (unsigned int i = 0 ; i < bufSize ; i++) {
        if(buf[i] == FLAG) {
            dataFrame = realloc(dataFrame,++frameSize);
            dataFrame[j++] = ESC;
            dataFrame[j++] = 0x5e;
        }
        else if (buf[i] == ESC) {
            dataFrame = realloc(dataFrame,++frameSize);
            dataFrame[j++] = ESC;
            dataFrame[j++]= 0x5d;
        }
        else
            dataFrame[j++] = buf[i];
    }
    dataFrame[j++] = BCC2;
    dataFrame[j++]= FLAG;


    printf("dataFrame: ");
    for (int i = 0; i < frameSize; i++) {
        printf("%02X ", dataFrame[i]);
    }
    printf("\n");
    int rr_byte=FALSE;
    int rej_byte=FALSE;
    int nnretransmissions = retransmissions;
    printf("retransmissions: %d\n", nnretransmissions); 
    while (nnretransmissions>=0) {
        alarmEnabled = FALSE;
        alarm(timeout);
        rr_byte=FALSE;
        rej_byte=FALSE;
        while (alarmEnabled == FALSE && !rr_byte && !rej_byte ) {
            writeBytesSerialPort(dataFrame, j);

            unsigned char res = updateStateMachineWrite();
            printf("res: %d fd:%d\n", res, fd);
            if (!res) {
                
                continue;
            }
            else if (res == C_REJ[0] || res == C_REJ[1]) {
                rej_byte = TRUE;
            }
            else if (res==C_RR[0] || res==C_RR[1]){
                rr_byte = TRUE;
                currentTramaTx = (currentTramaTx+1) % 2; 
            }
            else continue;
        }
        if (rr_byte) break;
        nnretransmissions--;
    }
    free(dataFrame);
    if (rr_byte) {
        return frameSize;
    }
    printf("Closing llclose in llwrite\n");
    llclose(1);
    return -1;
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    printf("Entering llread...\n");
    return updateStateMachineRead(packet);
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // mandar o F final e fechar a porta série
    StateLinkL state = START;
    unsigned char byte;
    (void) signal(SIGALRM, alarmHandler);
    
    unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_DISC, A^C_DISC, F};
    while(retransmissions != 0 && state != STOP) {
        
        if(writeBytesSerialPort(supervisionFrames, 5)!=5) {
            return -1;
        }
        
        alarmEnabled = FALSE;
        alarm(timeout);

        while (alarmEnabled == FALSE && state != STOP) {
            if (readByteSerialPort(&byte) > 0) {
                switch (state) {
                    case START:
                        if (byte == FLAG) state = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_RX) state = A_RCV;
                        else if (byte != FLAG) state = START;
                        break;
                    case A_RCV:
                        if (byte == C_DISC) state = C_RCV;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == (A_RX ^ C_DISC)) state = BCC1_OK;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case BCC1_OK:
                        if (byte == FLAG) state = STOP;
                        else state = START;
                        break;
                    default: 
                        break;
                }
            }
        }
        retransmissions--;
    }
    if (state != STOP)
        return -1;

    supervisionFrames[2] = C_UA;
    supervisionFrames[3] = A^C_UA;
    if(writeBytesSerialPort(supervisionFrames, 5)!=5) {
        return -1;
    }
    int clstat = closeSerialPort();
    return clstat;
}



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
            if (((byte ==A_RX^c_byte) && role == LlTx) || ((byte ==A^c_byte) && role == LlRx)) {
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
    int i = 0;
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

                    if (byte == (A_RX ^ cField)) state = BCC1_OK;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    printf("BCC1_OK a entrar\n");
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
    int i = 0;

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
