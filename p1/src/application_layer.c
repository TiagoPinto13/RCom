// Application layer protocol implementation
#include "link_layer.h"
#include "application_layer.h"
#include "helper_application_layer.h"
#include <string.h>
#include <stdlib.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayerRole layerRole;
    if (strcmp(role, "tx") == 0)
        layerRole = LlTx;
    else if (strcmp(role, "rx")==0)
        layerRole=LlRx;
    else{
        printf("error\n");
        return;
    }

    LinkLayer connect={
        .role=layerRole,
        .baudRate=baudRate,
        .nRetransmissions = nTries,
        .timeout= timeout
        };
    
    strcpy(connect.serialPort, serialPort);
    if (llopen(connect)<0){
        perror("Connec error\n");
        exit(-1);
    }
    unsigned char packet;
    printf("a entrar\n");
    unsigned char supervisionFrames[2] = {0x10,0xff};
    if(layerRole == LlTx){
        llwrite(supervisionFrames,2);
        printf("a sair\n");
        }
    else if (layerRole == LlRx) {
        printf("a entrar read");
        llread(&packet);
        printf("a sair2\n");
    }
    //printf(&packet);
    // printf ("llopen - done\n");
    // switch (layerRole)
    // {
    // case LlRx:
    //     unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
    //     int packetSize = -1;
    //     while ((packetSize = llread(packet)) < 0);
    //     unsigned long int rxFileSize = 0;
    //     unsigned char* name = parseControlPacket(packet, packetSize, &rxFileSize); 

    //     FILE* newFile = fopen((char *) name, "wb+");
    //     while (TRUE) {    
    //         while ((packetSize = llread(packet)) < 0);
    //         if(packetSize == 0) break;
    //         else if(packet[0] != 3){
    //             unsigned char *buffer = (unsigned char*)malloc(packetSize);
    //             parseDataPacket(packet, packetSize, buffer);
    //             fwrite(buffer, sizeof(unsigned char), packetSize-4, newFile);
    //             free(buffer);
    //         }
    //         else continue;
    //     }

    //     fclose(newFile);
    //     break;
        
    
    // case LlTx: {
    //     FILE* file = fopen(filename, "rb");
    //     if (file == NULL) {
    //         perror("File not found\n");
    //         exit(-1);
    //     }

    //     //calcula o tamanho do arquivo
    //     int prev = ftell(file);
    //     fseek(file,0L,SEEK_END);
    //     long int fileSize = ftell(file)-prev;
    //     fseek(file,prev,SEEK_SET);
        
        
    //     unsigned int cpSize;
    //     //constroi o packet inicial
    //     unsigned char *controlPacketStart = getControlPacket(2, filename, fileSize, &cpSize);
    //     // envia o packet
    //     if(llwrite(controlPacketStart, cpSize) == -1){ 
    //         printf("Exit: error in start packet\n");
    //         exit(-1);
    //     }
    //     printf("%ld",fileSize);
    //     unsigned char sequence = 0;
    //     //armazena no ponteiro content
    //     unsigned char* content = getData(file, fileSize);
    //     //bytes left são todos os ficheiros
    //     long int bytesLeft = fileSize;

    //     //while até enviar todos os bytes
    //     while (bytesLeft >= 0) { 
    //         //envia max bytes no payload
    //         int dataSize = bytesLeft > (long int) MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : bytesLeft;
    //         //aloca memoria para o proximo bloco de dados
    //         unsigned char* data = (unsigned char*) malloc(dataSize);
    //         memcpy(data, content, dataSize);
    //         int packetSize;
    //         //cria o packet a ser enviado
    //         unsigned char* packet = getDataPacket(sequence, data, dataSize, &packetSize);
    //         //envia o packet
    //         if(llwrite(packet, packetSize) == -1) {
    //             printf("Exit: error in data packets\n");
    //             exit(-1);
    //         }
            
    //         //atualiza os bytes left
    //         bytesLeft -= (long int) MAX_PAYLOAD_SIZE; 
    //         //move o ponteiro
    //         content += dataSize; 
    //         sequence = (sequence + 1) % 100;   
    //     }

    //     unsigned char *controlPacketEnd = getControlPacket(3, filename, fileSize, &cpSize);
    //     if(llwrite(controlPacketEnd, cpSize) == -1) { 
    //         printf("Exit: error in end packet\n");
    //         exit(-1);
    //     }
    //     llclose(1);
    //     break;
    // }
    
    // default:
    //     exit(-1);
    //     break;
    // }


}

