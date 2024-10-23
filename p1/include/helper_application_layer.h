#ifndef HELPER_APPLICATION_LAYER_H_
#define HELPER_APPLICATION_LAYER_H_

#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

unsigned char * getControlPacket(const unsigned int c, const char* filename, long int length, unsigned int* size);
unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize);
unsigned char * getDataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize);
void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer);
unsigned char * getData(FILE* fd, long int fileLength);

#endif