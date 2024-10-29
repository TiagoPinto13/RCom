// Application layer protocol implementation

#include "link_layer.h"
#include "application_layer.h"
#include <string.h>
#include <stdlib.h>
#include "application_layer_aux.h"

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
            
            unsigned char *begginingControlPacket = buildControlPacket(1, filename, fileSize, &SizeCP);
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

            unsigned char *endControlPacket = buildControlPacket(3, filename, fileSize, &SizeCP);
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

