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
#define A_rx 0x01
// #define C_RV 0x00
// #define BCC1 A^C_RV
// #define C_UA 0x07
// #define BCC1_UA A^C_UA

#define C_Set 0x03
#define C_UA 0x07
#define C_RR ((uint8_t[]){0xAA, 0xAB})
#define C_REJ ((uint8_t[]){0x54, 0x55})
#define C_Disc 0x0B
unsigned char c_byte;
int retransmissions = 0;
int timeout;
unsigned char currentTramaTx = 0;
extern int alarmEnabled;
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
    int retransmissions = connectionParameters.nRetransmissions;
    int timeout = connectionParameters.timeout;
    StateLinkL state = START;
    if (connectionParameters.role == LlTx && state==START ) {
        unsigned char byte;
        (void)signal(SIGALRM, alarmHandler);
        while (connectionParameters.nRetransmissions!=0 && state != STOP) {

            
            unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_Set, A^C_Set, F};
            if(writeBytesSerialPort(supervisionFrames, 5)<0) {
                return -1;
            }
            
            printf("supervisionframe wrote\n");

            alarm(connectionParameters.timeout);
            alarmEnabled = FALSE;        

            printf("try to read UA...\n");
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
        printf("UA received\n");
    }


    //parte para a pessoa que recebe

    else if (connectionParameters.role==LlRx){
        unsigned char byte;
        printf("try to read supervisionframe...\n");
        while (state != STOP)
        {   
            int rByte = readByteSerialPort(&byte);
            if (rByte<0)
                return -1;
            else if (rByte>0)
            {
                printf("byte detected - ");
                updateStateMachine(byte, &state, LlRx);
            }
            
        }
        printf("supervisionframe read\n");
        printf("sending UA\n");
        unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_UA, A^C_UA, F};
        writeBytesSerialPort(supervisionFrames, 5);
    }
    else
        return -1;
    printf("success final\n");
    return 0;

}



////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int frameSize = 6+bufSize;
    unsigned char *dataFrame = (unsigned char *) malloc(frameSize);
    dataFrame[0]=FLAG;
    dataFrame[1]=A;
    dataFrame[2]=C_Set;
    unsigned char BCC2 = buf[0];
    for (unsigned int i = 1; i<bufSize; i++) {
        BCC2^=buf[i];
    }
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
    if (BCC2 == FLAG) {
        dataFrame[j++] = 0x7d;
        dataFrame[j++] = 0x5e;
    }
    else
        dataFrame[j++] = BCC2;
    dataFrame[j++] = FLAG;
    int rr_byte=0;
    int rej_byte=0;
    int nnretransmissions = 0;
    while (nnretransmissions<retransmissions ) {
        alarmEnabled = FALSE;
        alarm(timeout);
        rr_byte=FALSE;
        rej_byte=FALSE;
        while (alarmEnabled == FALSE && !rr_byte && !rej_byte ) {
            writeBytesSerialPort(dataFrame, bufSize+5);
            unsigned char res = updateStateMachineWriteRead();
            if (res==0){
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
        nnretransmissions++;
    }

    if (rr_byte) {
        return frameSize;
    }

    return 0;
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // ler payload escrito pelo llwrite
    // F-> ESC|0x5E
    // ESC -> ESC| 0x5D
        return 0;
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
    
    unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_Disc, A^C_Disc, F};
    while(retransmissions != 0 && state != STOP) {
        
        if(writeBytesSerialPort(supervisionFrames, 5)!=5) {
            return -1;
        }
        alarmEnabled = FALSE;
        alarm(timeout);

        while (alarmEnabled == FALSE && state != STOP) {
            if (readByteSerialPort(&byte) > 0) {
                updateStateMachine(byte, &state, 0);
            }
        }
        retransmissions--;
    }
    if (state != STOP)
        return -1;

    //send another supervision frame??? why?
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
                printf("F read\n");
                *state = FLAG_RCV;
            }
            else printf("weird byte\n");
            break;

        case FLAG_RCV:
            if (byte == A && (role == LlTx || role == LlRx))
            {
                printf("A read\n");
                *state = A_RCV;
            }
            else if (byte == A_rx && role == NULL) {
                *state = A_RCV;
            }
            else if (byte != F) *state = START;

            else printf("weird byte\n");
            break;

        case A_RCV:
            if ((byte == C_Set && role == LlRx) || (byte == C_UA && role == LlTx) || (byte == C_Disc && role == NULL)) {
                printf("C_value read\n");
                *state = C_RCV;
                c_byte = byte;
            }
            
            else if (byte != F) *state = START;

            else if (byte == F) *state= FLAG_RCV;

            else printf("weird byte\n");

            break;


        case C_RCV:
            if (byte == (A^c_byte) && (role == LlTx || role == LlRx)) {
                printf("BCC read\n");
                *state = BCC1_OK;
            }
            else if (byte == A_rx^C_Disc && role == NULL) {
                *state = BCC1_OK;
            }
            else if (byte != F) *state = START;

            else if (byte == F) *state= FLAG_RCV;

            else printf("weird byte\n");

            break;
            

        case BCC1_OK:
            if (byte == F) {
                printf("final F read\n");
                *state = STOP;
            }
            else {
                *state = START;
                printf("weird byte\n");
            }
            break;
        
        default:
            break;
    }
    return 0;
}

int updateStateMachineWriteRead() {
    unsigned char byte, cField = 0;
    StateLinkL state = START;

    while (state != STOP && alarmEnabled == FALSE) {  
        if (readByteSerialPort(&byte) > 0 || 1) {
            switch (state)
            {
            case START:
            if (byte == FLAG) {
                state = FLAG_RCV;
            }
                break;
            
            case FLAG_RCV:
                if (byte == A_rx) state = A_RCV;
                else  if (byte != FLAG) state = START;
                break;

            case A_RCV:
                if(byte == C_REJ[0] || byte == C_REJ[1] || byte == C_RR[0] || byte == C_RR[1] || byte == C_Disc) {
                    state = C_RCV;
                    cField = byte;
                }
                else if (byte == FLAG) {
                    state = FLAG;
                }
                else state = START;
                break;

            case C_RCV:
                if (byte == (A_rx ^ cField)) state = BCC1_OK;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;

            case BCC1_OK:
                if (byte == FLAG){
                    state = STOP;
                }
                else state = START;
                break;
            default:
                break;
            }
        }
    }

    return cField;

}
