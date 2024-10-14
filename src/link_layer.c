// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int counter = 0;
int nAttempts;
int nTimeout;

// Alarm function handler
void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    counter++;
}

void enableAlarm(int time) {
    alarm(time);
    alarmEnabled = TRUE;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    int fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);

    if (fd < 0) {
        return -1;
    }

    nAttempts = connectionParameters.nRetransmissions;
    nTimeout = connectionParameters.timeout;

    if(connectionParameters.role == LlRx) {
        unsigned char buf[BUF_SIZE + 1] = {0};

        state_t state = START_SM;
        unsigned char rcv;
        
        while (state != STOP_SM) {
            int bytes = readByteSerialPort(&rcv);
            if (bytes <= 0) {
                return -1; // Error reading from serial port
            }
            switch (state) {
                case START_SM:
                    if (rcv == FRAME) state = FLAG_OK;
                    break;

                case FLAG_OK:
                    if (rcv == RECIEVER_ADDRESS) state = A_OK;
                    else if (rcv != FRAME) state = START_SM;
                    break;

                case A_OK:
                    if (rcv == SET) state = C_OK;
                    else if (rcv == FRAME) state = FLAG_OK;
                    else state = START_SM;
                    break;

                case C_OK:
                    if (rcv == (RECIEVER_ADDRESS ^ SET)) state = BCC_OK;
                    else if (rcv == FRAME) state = FLAG_OK;
                    else state = START_SM;
                    break;

                case BCC_OK:
                    if (rcv == FRAME) state = STOP_SM;
                    else state = START_SM;
                    break;

                default:
                    return -1;
            }
        }

        // Send UA response frame
        buf[0] = FRAME;
        buf[1] = RECIEVER_ADDRESS;
        buf[2] = UA;
        buf[3] = (buf[1] ^ buf[2]);
        buf[4] = FRAME;

        if (writeBytesSerialPort(buf, 5) < 0) {
            return -1;
        }

        return fd;
    }

    else if(connectionParameters.role == LlTx) {
        unsigned char buf[BUF_SIZE + 1] = {0};

        (void)signal(SIGALRM, alarmHandler);

        buf[0] = FRAME;
        buf[1] = TRANSMITER_ADDRESS;
        buf[2] = SET;
        buf[3] = (buf[1] ^ buf[2]);
        buf[4] = FRAME;

        int attempts = connectionParameters.nRetransmissions;

        while (attempts != 0) {
            // Send the SET frame
            if (writeBytesSerialPort(buf, 5) < 0) {
                return -1;
            }

            // Wait for UA response (timeout logic)
            alarm(connectionParameters.timeout);  // Set a 3-second timeout (implement this using the alarmHandler function)
            alarmEnabled = TRUE;

            state_t state = START_SM;
            unsigned char rcv;

            while (state != STOP_SM && alarmEnabled) {
                int bytes = readByteSerialPort(&rcv);
                if (bytes <= 0) {
                    return -1;  // Error reading from serial port
                }

                switch (state) {
                    case START_SM:
                        if (rcv == FRAME) state = FLAG_OK;
                        break;

                    case FLAG_OK:
                        if (rcv == TRANSMITER_ADDRESS) state = A_OK;
                        else if (rcv != FRAME) state = START_SM;
                        break;

                    case A_OK:
                        if (rcv == UA) state = C_OK;
                        else if (rcv == FRAME) state = FLAG_OK;
                        else state = START_SM;
                        break;

                    case C_OK:
                        if (rcv == (TRANSMITER_ADDRESS ^ SET)) state = BCC_OK;
                        else if (rcv == FRAME) state = FLAG_OK;
                        else state = START_SM;
                        break;

                    case BCC_OK:
                        if (rcv == FRAME) state = STOP_SM;
                        else state = START_SM;
                        break;

                    default:
                        return -1;
                }
            }

            if (state == STOP_SM) {
                alarm(0);  // Cancel the alarm
                return fd;  // Success, connection established
            }

            attempts--;
        }
        return -1;
    }
    else return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize){
    // TODO
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet){
    // TODO
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {

    state_t state = START_SM;
    (void)signal(SIGALRM, alarmHandler);

    unsigned char rcv;
    
    while (nAttempts != 0 && state != STOP_SM){
        
        unsigned char CLOSE_WORD[5] = {FRAME, TRANSMITER_ADDRESS, DISCONNECT, (TRANSMITER_ADDRESS ^ DISCONNECT) ,FRAME};
        if(writeBytesSerialPort(CLOSE_WORD,5) < 0) return -1;

        while (alarmEnabled) {
            if (readByteSerialPort(&rcv) > 0) {
                switch (state) {
                    case START_SM:
                        if (rcv == FRAME) state = FLAG_OK;
                        break;

                    case FLAG_OK:
                        if (rcv == RECIEVER_ADDRESS) state = A_OK;
                        else if (rcv != FRAME) state = START_SM;
                        break;

                    case A_OK:
                        if (rcv == DISCONNECT) state = C_OK;
                        else if (rcv == FRAME) state = FLAG_OK;
                        else state = START_SM;
                        break; 

                    case C_OK:
                        if (rcv == (RECIEVER_ADDRESS ^ DISCONNECT)) state = BCC_OK;
                        else if (rcv == FRAME) state = FLAG_OK;
                        else state = START_SM;
                        break;

                    case BCC_OK:
                        if (rcv == FRAME) state = STOP_SM;
                        else state = START_SM;
                        break;

                    default:
                        return -1;
                }
            }
        }
        nAttempts--;
    }

    if(state != STOP_SM) return -1;

    unsigned char CLOSE_WORD[5] = {FRAME, TRANSMITER_ADDRESS, UA, TRANSMITER_ADDRESS ^ UA,FRAME};
    if(writeBytesSerialPort(CLOSE_WORD,5) < 0){
        return -1;
    }

    int clstat = closeSerialPort();
    return clstat;
}
