/**
 * @file application_layer_aux.h
 * @brief Auxiliary functions for Application Layer protocol operations.
 *
 * This file contains declarations of helper functions for handling control and data packet
 * creation and parsing in the Application Layer protocol. These functions facilitate building
 * and interpreting packet structures to support file transfer operations.
 */

#ifndef _APPLICATION_LAYER_AUX_H_
#define _APPLICATION_LAYER_AUX_H_

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/**
 * @brief Builds a control packet for the application layer.
 *
 * This function creates a control packet containing information about the file to be transferred,
 * including a control byte, file size, and filename. It calculates the number of bytes required 
 * for each field and assembles the packet accordingly.
 *
 * @param c Control byte indicating packet type (e.g., START, END).
 * @param filename Name of the file to be transferred.
 * @param filesize Size of the file in bytes.
 * @param length Pointer to store the total size of the generated control packet.
 * @return Pointer to the dynamically allocated control packet.
 */
unsigned char* buildControlPacket(const unsigned int c, const char* filename, long int filesize, unsigned int* length);

/**
 * @brief Builds a data packet with a sequence number and data payload.
 *
 * This function generates a data packet that includes a sequence number and a specified data payload.
 * The packet includes headers for control and sequence number, followed by the data size in two bytes,
 * and then the actual data payload.
 *
 * @param sequenceNumber Sequence number of the data packet.
 * @param data Pointer to the data to be included in the packet.
 * @param dataSize Size of the data in bytes.
 * @param packetSize Pointer to store the total size of the generated data packet.
 * @return Pointer to the dynamically allocated data packet.
 */
unsigned char* buildDataPacket(unsigned char sequenceNumber, unsigned char *data, int dataSize, int *packetSize);

/**
 * @brief Parses a control packet to extract file information.
 *
 * This function reads a control packet to retrieve the file size and file name.
 * It extracts the file size using its encoded byte length and returns a dynamically
 * allocated string for the filename.
 *
 * @param packet The control packet to parse.
 * @param size The size of the control packet.
 * @param fileSize Pointer to store the extracted file size.
 * @return Pointer to a dynamically allocated array containing the filename, or NULL on error.
 */
unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize);

/**
 * @brief Parses a data packet to extract its contents.
 *
 * This function reads a data packet and copies the payload into a provided buffer.
 * The buffer is filled starting at the specified packet position, excluding the header.
 *
 * @param packet The data packet to parse.
 * @param packetSize The size of the data packet.
 * @param buffer Buffer to store the extracted data.
 */
void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer);

/**
 * @brief Reads the content of a file and returns it as a data buffer.
 *
 * This function reads the entire content of the specified file into a buffer,
 * allocating memory dynamically based on the file length. The file must be open and readable.
 *
 * @param fd File pointer to the file to be read.
 * @param fileLength Length of the file in bytes.
 * @return Pointer to the dynamically allocated data buffer containing the file content.
 */
unsigned char* buildData(FILE* fd, long int fileLength);

#endif // _APPLICATION_LAYER_AUX_H_
