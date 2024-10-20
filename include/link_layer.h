// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

// F
#define FRAME              0x7E

// A
#define TRANSMITER_ADDRESS 0x03 
#define RECIEVER_ADDRESS   0x01 

// C 
#define SET                0x03
#define UA                 0x07
#define C_RR0              0xAA
#define C_RR1              0xAB
#define C_REJ0             0x54
#define C_REJ1             0x55
#define DISCONNECT         0x0B

// Byte Stuffing Control
#define ESCAPE             0x7D
#define ESCAPE2            0x5E
#define ESCAPE3            0x5D

// ALARM
#define SIGALRM 14

typedef enum {
	START_SM,
	FLAG_OK,
	A_OK,
	C_OK,
	BCC_OK,
	STOP_SM,
    DATA,
    DATA_ESCAPE
} state_t;

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

unsigned char calculateBCC2(const unsigned char *data, int length);

int byteStuffing(unsigned char *frame, int length);

int byteDestuffing(unsigned char *frame, int length);

#endif // _LINK_LAYER_H_
