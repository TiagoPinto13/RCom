// Application layer protocol implementation

#include "link_layer.h"
#include "application_layer.h"
#include <string.h>
#include <stdlib.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{

    LinkLayerRole layerRole;
    if (strcmp(role, "tx") == 0)
        layerRole = LlTx;
    else if (strcmp(role, "rx") == 0)
        layerRole = LlRx;
    else{
        printf("Error: Invalid role specified.\n");
        return;
    }

    LinkLayer connect = {
        .role = layerRole,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout
    };
    
    strcpy(connect.serialPort, serialPort);

    printf("Debug: Attempting to open connection on %s as %s.\n", serialPort, role);
    
    //while (llopen(connect) < 0);
    if (llopen(connect) < 0){
        perror("Error: Connection failed.\n");
        exit(-1);
    }

    switch (layerRole) {
        case LlRx: {
            printf("Debug: Receiver mode started.\n");
            unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
            int packetSize = -1;
            while ((packetSize = llread(packet)) < 0);
            printf("Debug: Received start control packet of size %d.\n", packetSize);

            unsigned long int FileSizeRx = 0;
            unsigned char* name = parseControlPacket(packet, packetSize, &FileSizeRx); 
            printf("Debug: Control packet parsed. File name: %s, Size: %lu.\n", name, FileSizeRx);
            printf("file name: %s\n", filename);
            FILE* newFile = fopen(filename, "wb+");

            if (newFile == NULL) {
                perror("Error: Could not open file for writing.\n");
                exit(-1);
            }

            while (TRUE) {
                while ((packetSize = llread(packet)) < 0);
                printf("Debug: Received packet of size %d.\n", packetSize);

                if (packetSize == 0) break;
                else if (packet[0] != 3) {
                    unsigned char *buffer = (unsigned char*)malloc(packetSize);
                    parseDataPacket(packet, packetSize, buffer);
                    fwrite(buffer, sizeof(unsigned char), packetSize-4, newFile);
                    free(buffer);
                } else {
                    printf("Debug: End control packet received.\n");
                    continue;
                }
            }

            fclose(newFile);
            break;
        }
            
        case LlTx: {
            printf("Debug: Transmitter mode started.\n");
            FILE* file = fopen(filename, "rb");
            if (file == NULL) {
                perror("Error: Could not open file for reading.\n");
                exit(-1);
            }

            fseek(file, 0, SEEK_END);
            long int fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);
            

        
            unsigned int SizeCP;
            
            unsigned char *begginingControlPacket = buildCtrlPacket(1, filename, fileSize, &SizeCP);
            for (unsigned int i = 0; i < SizeCP; i++) {
                printf("%02X ", begginingControlPacket[i]);  // Imprimir em hexadecimal
            }
            printf("\n");
            if (llwrite(begginingControlPacket, SizeCP) == -1) {
                printf("Error: Failed to send start control packet.\n");
                exit(-1);
            }
            printf("Debug: Sent start control packet.\n");

            unsigned char sequenceNumber = 0;
            unsigned char* fullContent = buildData(file, fileSize);
            printf("FILE SIZE: %ld\n", fileSize);
            long int leftBytes = fileSize;

            while (leftBytes > 0) {
                int dataSize = leftBytes > (long int)MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : leftBytes;
                unsigned char* data = (unsigned char*)malloc(dataSize);
                memcpy(data, fullContent, dataSize);

                int packetSize;
                unsigned char* packet = buildDataPacket(sequenceNumber, data, dataSize, &packetSize);
                if (llwrite(packet, packetSize) == -1) {
                    printf("Error: Failed to send data packet.\n");
                    exit(-1);
                }

                printf("Debug: Sent packet sequenceNumber %d of size %d.\n", sequenceNumber, dataSize);
                leftBytes -= dataSize;
                fullContent += dataSize;
                sequenceNumber = (sequenceNumber + 1) % 100;

                free(data);
            }

            unsigned char *endControlPacket = buildCtrlPacket(3, filename, fileSize, &SizeCP);
            if (llwrite(endControlPacket, SizeCP) == -1) {
                printf("Error: Failed to send end control packet.\n");
                exit(-1);
            }

            //printf("Debug: File transmission complete.\n");
            llclose(1);
            break;
        }

        default:
            printf("Error: Invalid role.\n");
            exit(-1);
            break;
    }
}
unsigned char *buildCtrlPacket(const unsigned int c, const char *filename, long int filesize, unsigned int *length){
    int L1 = 0;
    int L2 = strlen(filename);
    int packetpos = 0;
    long int filesizeAux = filesize;

    while(filesizeAux > 0){
        L1++;
        filesizeAux= filesizeAux >> 8;
    }

    *length = 3 + L1 + 2 + L2; // 3 bytes for C, T1 and L1, L1 bytes for V1, 1 byte for T2, L2 bytes for V2

    unsigned char *packet = (unsigned char*)malloc(*length);

    packet[packetpos] = c;
    packetpos++;
    packet[packetpos] = 0;
    packetpos++;
    packet[packetpos] = L1;
    
    for(int i = L1 + packetpos; i > packetpos; i--){
        packet[i] = (0XFF & filesize);
        filesize >>= 8;
    }

    packetpos += L1 + 1;
    
    packet[packetpos] = 1;
    
    packetpos++;
    
    packet[packetpos] = L2;
    packetpos++;
    for(int j = 0; j < L2; j++){
        packet[packetpos + j] = filename[j];
    }
    return packet;
}

unsigned char * buildDataPacket(unsigned char sequenceNumber, unsigned char *data, int dataSize, int *packetSize){

    *packetSize = 4 + dataSize;
    unsigned char* packet = (unsigned char*)malloc(*packetSize);

    packet[0] = 2;   
    packet[1] = sequenceNumber;
    packet[2] = (dataSize >> 8) & 0xFF;
    packet[3] = dataSize & 0xFF;
    memcpy(packet+4, data, dataSize);
    printf("Packet fullContent: ");
    for (int i = 0; i < *packetSize; i++) {
        printf("%02X ", packet[i]); 
    }
    printf("\n");
    return packet;
}


unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {

    unsigned char fileSizeNBytes = packet[2];
    unsigned char fileSizeAux[fileSizeNBytes];
    memcpy(fileSizeAux, packet+3, fileSizeNBytes);
    for(unsigned int i = 0; i < fileSizeNBytes; i++)
        *fileSize |= (fileSizeAux[fileSizeNBytes-i-1] << (8*i));

    unsigned char fileNameNBytes = packet[3+fileSizeNBytes+1];
    unsigned char *name = (unsigned char*)malloc(fileNameNBytes);
    memcpy(name, packet+3+fileSizeNBytes+2, fileNameNBytes);
    return name;
}

void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    memcpy(buffer,packet+4,packetSize-4);
    buffer += packetSize+4;
}

unsigned char * buildData(FILE* fd, long int fileLength) {
    unsigned char* fullContent = (unsigned char*)malloc(sizeof(unsigned char) * fileLength);
    fread(fullContent, sizeof(unsigned char), fileLength, fd);
    return fullContent;
}
