/**
 * @file link_layer_aux.h
 * @brief Auxiliary functions for the Link Layer protocol's state machine operations.
 *
 * This file contains functions to handle the state machine for both reading and writing frames
 * in the Link Layer protocol. The functions manage state transitions for each byte received and
 * perform error handling with retransmission capabilities if needed.
 *
 * 
 * 
 * 
 * Since modifications to link_layer.h are restricted, auxiliary functions for the link layer
 * are declared here and implemented in link_layer_aux.c.
 */

#ifndef _LINK_LAYER_AUX_H_
#define _LINK_LAYER_AUX_H_

#include "link_layer.h"
#include <stdio.h>

/** Buffer size for link-layer data processing */
#define BUF_SIZE 1024
/** Write buffer size for link-layer operations */
#define WRITE_SIZE 5
/** Alarm timeout duration in seconds */
#define ALARM_SECONDS 3
/** Flag byte used in link-layer frames */
#define FLAG 0x7E
/** Escape byte used in link-layer frames */
#define ESC 0x7D
/** Frame delimitator (same as FLAG) */
#define F 0x7E
/** Address field for sender-receiver communication */
#define A 0x03
/** Address field for receiver-sender communication */
#define A_RX 0x01

/** Control field for SET frame */
#define C_Set 0x03
/** Control field for UA (Unnumbered Acknowledge) frame */
#define C_UA 0x07
/** Control fields for Receiver Ready (RR) frames */
#define C_RR ((uint8_t[]){0xAA, 0xAB})
/** Control fields for Reject (REJ) frames */
#define C_REJ ((uint8_t[]){0x54, 0x55})
/** Control fields for Frame Number (FN) frames */
#define C_FN ((uint8_t[]){0x00, 0x80})
/** Control field for DISC (Disconnect) frame */
#define C_DISC 0x0B


// External variables representing the current configuration and control state
extern unsigned char c_byte;
extern int retransmissions;
extern int timeout;
extern unsigned char currentTramaTx;
extern int alarmEnabled;
extern int fd;
extern char serialPort[50];
extern int baudRate;




/**
 * @enum StateLinkL
 * @brief States used in the link-layer finite state machines.
 *
 * These states are used to keep track of the link-layer frame parsing process.
 */
typedef enum
{
    START,      /**< Initial state */
    FLAG_RCV,   /**< Flag byte received */
    A_RCV,      /**< Address byte received */
    C_RCV,      /**< Control byte received */
    BCC1_OK,    /**< BCC1 verification passed */
    STOP,       /**< End of frame received */
    READING,    /**< Currently reading a frame */
    FOUND_ESC   /**< Escape character detected */
} StateLinkL;

/**
 * @brief Updates the Link Layer state machine based on a single received byte.
 *
 * This function processes an incoming byte to update the state of the Link Layer
 * for managing frame reception, transitioning between states (START, FLAG_RCV, A_RCV,
 * C_RCV, BCC1_OK, STOP) based on the byte values and protocol roles (LlTx or LlRx).
 *
 * @param byte The received byte to process.
 * @param state Pointer to the current state of the state machine.
 * @param role The role of the Link Layer (LlTx for transmitter, LlRx for receiver).
 * @return Returns 0 on success.
 */
int updateStateMachine(unsigned char byte, StateLinkL *state, LinkLayerRole role);

/**
 * @brief Executes the state machine to manage outgoing frame transmission.
 *
 * This function reads bytes from the serial port and updates the state machine
 * for frame transmission. If an error occurs (e.g., port disconnect), the serial
 * port is reopened, and transmission restarts. The function handles setting up frames
 * and completing frame transmission.
 *
 * @return The control field of the frame received or -1 if an error occurs.
 */
int updateStateMachineWrite();

/**
 * @brief Executes the state machine to manage incoming frame reception.
 *
 * This function reads incoming bytes from the serial port and processes each byte
 * through the state machine to parse frames. If a complete frame is received,
 * it verifies the BCC2 for error-checking and sends an acknowledgment (C_RR).
 * If BCC2 validation fails, a rejection frame (C_REJ) is sent.
 *
 * @param packet Pointer to store the received frame's content.
 * @return The size of the received packet if successfully read; -1 on error.
 */
int updateStateMachineRead();

#endif // _LINK_LAYER_AUX_H_
