// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "alarm.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>




// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 1024
#define WRITE_SIZE 5
#define ALARM_SECONDS 3
#define FLAG 0x7E
#define ESC 0x7D
#define F 0x7E
#define A 0x03
// #define C_RV 0x00
// #define BCC1 A^C_RV
// #define C_UA 0x07
// #define BCC1_UA A^C_UA

unsigned char c_byte;
unsigned char C_values[2] = {0x00, 0x07, 0x03, 0xAA, 0xAB, 0x54, 0x55, 0x0B};
int retransmissions = 0;
int timeout;
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

            
            unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_values[0], A^C_values[0], F};
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
        unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_values[1], A^C_values[1], F};
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
    dataFrame[2]=C_values[0];
    unsigned char BCC2 = buf[0];
    for (unsigned int i = 1; i<bufSize; i++) {
        BCC2^=buf[i];
    }
    int j = 4;
    for (unsigned int i = 0 ; i < bufSize ; i++) {
        if(buf[i] == FLAG || buf[i] == ESC) {
            dataFrame = realloc(dataFrame,++frameSize);
            dataFrame[j++] = ESC;
        }
        dataFrame[j++] = buf[i];
    }
    dataFrame[j++] = BCC2;
    dataFrame[j++] = FLAG;
    
    writeBytesSerialPort(dataFrame, bufSize+5);
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
    unsigned char byte;
    unsigned char supervisionFrames[BUF_SIZE] = {F, A, C_values[6], A^C_values[6], F};
    while(retransmissions != 0 && state != STOP) {
        
        if(writeBytesSerialPort(supervisionFrames, 5)!=5) {
            return -1;
        }
        alarmEnabled = FALSE;
        alarm(timeout);

        while (alarmEnabled == FALSE && state != STOP) {
            if (readByteSerialPort(&byte) > 0) {
                updateStateMachine(byte, &state, NULL);
            }
        }
        retransmissions--;
    }
    if (state != STOP)
        return -1;

    //send another supervision frame??? why?
    supervisionFrames[2] = C_values[1];
    supervisionFrames[3] = A^C_values[1];
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
            else if (byte == 0x01 && role == NULL) {
                *state = A_RCV;
            }
            else if (byte != F) *state = START;

            else printf("weird byte\n");
            break;

        case A_RCV:
            if ((byte == C_values[0] && role == LlRx) || (byte == C_values[1] && role == LlTx) || (byte == C_values[6] && role == NULL)) {
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
            else if (byte == 0x01^C_values[6] && role == NULL) {
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


// int updateStateMachine2(unsigned char byte, StateLinkL *state) {
//     switch (*state)
//     {
//         case START:
//             if (byte == F)
//             {
//                 printf("F read\n");
//                 *state = FLAG_RCV;
//             }
//             break;

//         case FLAG_RCV:
//             if (byte == A)
//             {
//                 printf("A read\n");
//                 *state = A_RCV;
//             }
//             else if (byte != F) *state = START;
//             break;

//         case A_RCV:
//             if (byte == C_UA) {
//                 printf("C_UA read\n");
//                 *state = C_RCV;
//                 c_byte = byte;
//             }
            
//             else if (byte != F) *state = START;

//             else if (byte == F) *state= FLAG_RCV;

//             break;


//         case C_RCV:
//             if (byte == (BCC1_UA)) {
//                 printf("BCC1 read\n");
//                 *state = BCC1_OK;
//             }
//             else if (byte != F) *state = START;

//             else if (byte == F) *state= FLAG_RCV;

//             break;
            

//         case BCC1_OK:
//             if (byte == F) {
//                 printf("final F read\n");
//                 *state = STOP;
//             }
//             else *state = START;
//             break;
        
//         default:
//             break;
//     }
//     return 0;
// }

