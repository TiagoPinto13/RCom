

#include "application_layer_aux.h"

unsigned char *buildControlPacket(const unsigned int c, const char *filename, long int filesize, unsigned int *length){
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

