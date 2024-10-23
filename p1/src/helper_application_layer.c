#include "link_layer.h"
#include "helper_application_layer.h"


unsigned char * getControlPacket(const unsigned int c, const char* filename, long int length, unsigned int* size){

    const int L1 = (int) length/8.0; //dar fix
    const int L2 = strlen(filename);
    *size = 1+2+L1+2+L2;
    unsigned char *packet = (unsigned char*)malloc(*size);
    
    unsigned int pos = 0;
    packet[pos++]=c;
    packet[pos++]=0;
    packet[pos++]=L1;

    for (unsigned char i = 0 ; i < L1 ; i++) {
        packet[2+L1-i] = length & 0xFF;
        length >>= 8;
    }
    pos+=L1;
    packet[pos++]=1;
    packet[pos++]=L2;
    memcpy(packet+pos, filename, L2);
    return packet;
}

unsigned char * getDataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize){

    *packetSize = 1 + 1 + 2 + dataSize;
    unsigned char* packet = (unsigned char*)malloc(*packetSize);

    packet[0] = 1;   
    packet[1] = sequence;
    packet[2] = dataSize >> 8 & 0xFF;
    packet[3] = dataSize & 0xFF;
    memcpy(packet+4, data, dataSize);

    return packet;
}


unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {

    // File Size
    unsigned char fileSizeNBytes = packet[2];
    unsigned char fileSizeAux[fileSizeNBytes];
    memcpy(fileSizeAux, packet+3, fileSizeNBytes);
    for(unsigned int i = 0; i < fileSizeNBytes; i++)
        *fileSize |= (fileSizeAux[fileSizeNBytes-i-1] << (8*i));

    // File Name
    unsigned char fileNameNBytes = packet[3+fileSizeNBytes+1];
    unsigned char *name = (unsigned char*)malloc(fileNameNBytes);
    memcpy(name, packet+3+fileSizeNBytes+2, fileNameNBytes);
    return name;
}

void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    memcpy(buffer,packet+4,packetSize-4);
    buffer += packetSize+4;
}

unsigned char * getData(FILE* fd, long int fileLength) {
    unsigned char* content = (unsigned char*)malloc(sizeof(unsigned char) * fileLength);
    fread(content, sizeof(unsigned char), fileLength, fd);
    return content;
}
