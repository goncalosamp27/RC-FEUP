// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "unistd.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

volatile int STOP = FALSE;

int alarmTrigger = FALSE;
int counter = 0;
int nAttempts; // should be final, not changed
int nTimeout; // should be final, not changed

// Alarm function handler
void alarmHandler(int signal) {
    alarmTrigger = TRUE;
    counter++;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {

    int fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);

    if(fd < 0){
        return -1; //connection error
    }

    nAttempts = connectionParameters.nRetransmissions;
    nTimeout = connectionParameters.timeout;

    state_t state = START_SM;
    unsigned char rcv;

    if(connectionParameters.role == LlTx){
        (void) signal(SIGALRM, alarmHandler);
        
        while(state != STOP_SM && nAttempts != 0){
            alarm(nTimeout);
            alarmTrigger = FALSE;

            unsigned char buf[5];
            buf[0] = FRAME;
            buf[1] = TRANSMITER_ADDRESS;
            buf[2] = SET;
            buf[3] = (buf[1] ^ buf[2]);
            buf[4] = FRAME;

            if(writeBytesSerialPort(buf, sizeof(buf)) == -1){
                printf("foi aqui 1\n");
                return -1;
            }

            while(alarmTrigger == FALSE && state != STOP_SM){
                if(readByteSerialPort(&rcv) == -1){
                    printf("foi aqui 2\n");
                    return -1;
                }
                else{
                    switch (state) {
                        case START_SM:
                            if (rcv == FRAME) state = FLAG_OK;
                            break;

                        case FLAG_OK:
                            if (rcv == RECIEVER_ADDRESS) state = A_OK;
                            else if (rcv != FRAME) state = START_SM;
                            break;

                        case A_OK:
                            if (rcv == UA) state = C_OK;
                            else if (rcv == FRAME) state = FLAG_OK;
                            else state = START_SM;
                            break;

                        case C_OK:
                            if (rcv == (RECIEVER_ADDRESS ^ UA)) state = BCC_OK;
                            else if (rcv == FRAME) state = FLAG_OK;
                            else state = START_SM;
                            break;

                        case BCC_OK:
                            if (rcv == FRAME) state = STOP_SM;
                            else state = START_SM;
                            break;
                        
                        default:
                            break;
                    }
                }
            }
            nAttempts--;
        }
        if(state != STOP_SM){
            printf("foi aqui 3\n");
            return -1;
        }
    } 
    else if(connectionParameters.role == LlRx){
        while(state != STOP_SM){
            if(readByteSerialPort(&rcv) == -1){
                    printf("foi aqui deu\n");
                    return -1;
            }
            else{
                switch (state) {
                    case START_SM:
                        if (rcv == FRAME) state = FLAG_OK;
                        break;

                    case FLAG_OK:
                        if (rcv == TRANSMITER_ADDRESS) state = A_OK;
                        else if (rcv != FRAME) state = START_SM;
                        break;

                    case A_OK:
                        if (rcv == SET) state = C_OK;
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
                        break;
                }
            }
        }

        unsigned char buf[5];
        buf[0] = FRAME;
        buf[1] = RECIEVER_ADDRESS;
        buf[2] = UA;
        buf[3] = (buf[1] ^ buf[2]);
        buf[4] = FRAME;

        if(writeBytesSerialPort(buf, sizeof(buf)) == -1){
            return -1;
        }
    }
    else {
        return -1;
    }
    printf("chegou ao fim\n");
    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    int frame_size = 6 + bufSize;
    unsigned char *frame = (unsigned char *) malloc(frame_size); // alocar espaço na frame

    frame[0] = FRAME; // FLAG
    frame[1] = TRANSMITER_ADDRESS; // A
    frame[2] = (0 << 6); // C
    frame[3] = (frame[1] ^ frame[2]); // BCC1

    // STUFFING //
    current_byte = 4; // nao podemos escrever nos bytes ja ocupados frame[0,1,2,3]
    for (int i = 0 ; i < bufSize ; i++) 
    {
        if (buf[i] == FRAME || buf[i] == ESCAPE) {
            frame_size += 1; 
            frame = realloc(frame, frame_size); // aumenta a size da frame em 1 para os escape macros
            
            frame[current_byte] = ESCAPE; 
            current_byte++;
            
            frame[current_byte] = (buf[i] == FRAME) ? ESCAPE2 : ESCAPE3;
            current_byte++;
        }
        else {
            frame[current_byte] = buf[i];
            current_byte++;
        }
    }

    unsigned char BCC2;

    for(int i = 0; i < bufSize; i++) 
    {
        if (i == 0) BCC2 = buf[0];
        else BCC2 = BCC2 ^ buf[i];
    }

    frame[current_byte] = BCC2; 
    current_byte++;
    frame[current_byte] = FLAG;

    int nTransmition = 0;
    bool rejected = false; 
    bool accepted = false;

    while(nTransmition < nAttempts) {

        alarmTrigger = FALSE;
        alarm(nTimeout);

        while(alarmTrigger == FALSE && !rejected && !accepted) {
            if(writeBytesSerialPort(frame, frame_size) == -1) return -1;
        }
    }

    return frame_size;
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
