// Link layer protocol implementation

#include "link_layer.h"
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
#include <time.h>



// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

unsigned char c_byte;
int retransmissions = 0;
int timeout;
unsigned char currentTramaTx = 0;
extern int alarmEnabled;
extern int fd;
char serialPort[50]; 
int baudRate;
int totalPacketsSent = 0;
int totalPacketsReceived = 0;
int totalRetransmissions = 0;
double totalTransferTime = 0.0;
clock_t startTime, endTime;


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
    startTime = clock();

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
        
        unsigned char supervisionFrames[BUF_SIZE] = {F, A_RX, C_UA, (A_RX^C_UA), F};
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
    clock_t packetStartTime = clock();
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
        if (rej_byte) {
            nnretransmissions = retransmissions;
        }
        alarmEnabled = FALSE;
        alarm(timeout);
        rr_byte=FALSE;
        rej_byte=FALSE;
        while (alarmEnabled == FALSE && !rr_byte && !rej_byte ) {
            writeBytesSerialPort(dataFrame, j);
            totalPacketsSent++;
            unsigned char res = updateStateMachineWrite();
            printf("res: %d fd:%d\n", res, fd);
            if (!res) {
                
                continue;
            }
            else if (res == C_REJ[0] || res == C_REJ[1]) {
                rej_byte = TRUE;
                totalRetransmissions++;
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
    totalTransferTime += (double)(clock() - packetStartTime) / CLOCKS_PER_SEC;
    free(dataFrame);
    if (rr_byte) {
        return frameSize;
    }
    printf("Closing llclose in llwrite\n");
    printf("totalpacketsReceived: %d\n", totalPacketsReceived);
    llclose(1);
    return -1;
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    printf("Entering llread...\n");
    clock_t packetStartTime = clock();
    int result = updateStateMachineRead(packet);
    if(result > 0){    
        totalPacketsReceived++;
    }
    totalTransferTime += (double)(clock() - packetStartTime) / CLOCKS_PER_SEC;

    return result;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    endTime = clock();
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
    if (showStatistics) {
        int time= endTime - startTime;
        double totalExecutionTime = (double)(time) / CLOCKS_PER_SEC;

        printf("Connection Statistics:\n");
        printf("Total Packets Sent: %d\n", totalPacketsSent);
        //printf("Total Packets Received: %d\n", totalPacketsReceived);
        printf("Total Retransmissions: %d\n", totalRetransmissions);
        printf("Total Transfer Time (packets): %.2f seconds\n", totalTransferTime);
        printf("Total Execution Time: %.2f seconds\n", totalExecutionTime);
    }
    int clstat = closeSerialPort();
    return clstat;
}


