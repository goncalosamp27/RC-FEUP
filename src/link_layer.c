// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "unistd.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

volatile int STOP = FALSE;

int alarmTrigger = FALSE;
int counter = 0;
int nAttempts; // should be final, not changed
int nTimeout; // should be final, not changed

unsigned char control_C_RX = 1;
unsigned char control_C_TX = 0;

// Alarm function handler
void alarmHandler(int signal) {
    alarmTrigger = TRUE;
    counter++;
}

extern int fd;
extern LinkLayer linklayer;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {

    int fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);

    if(fd < 0){
        perror("Connecting erro");
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

unsigned char calculateBCC2(const unsigned char *data, int length) {
    unsigned char bcc2 = 0x00;

    for (int i = 0; i < length; i++) {
        bcc2 ^= data[i];  // XOR para cada byte
    }
    return bcc2;
}

int llwrite(const unsigned char *buf, int bufSize) {

    unsigned char BCC2 = calculateBCC2(buf, bufSize);

    // int frame_size = 6 + bufSize; // 6 = 4 posições iniciais + 2 finais, somamos o size do buffer
    // unsigned char *frame = (unsigned char *) malloc(frame_size); // alocar espaço na frame

    // frame[0] = FRAME;                 // FLAG
    // frame[1] = TRANSMITER_ADDRESS;    // A
    // frame[2] = (control_C_TX << 7);              // C
    // frame[3] = (frame[1] ^ frame[2]); // BCC1

    unsigned char *dataBCC2 = (unsigned char *)malloc(2 * bufSize + 2);
    if(!dataBCC2){
        //erro na alocação
        return -1;
    }

    // STUFFING // 

    int i = 0;
    for(int j = 0; j < bufSize; i++){
        if(buf[j] == FRAME){
            dataBCC2[i++] = ESCAPE;
            dataBCC2[i++] = ESCAPE2;
        } else if(buf[j] == ESCAPE){
            dataBCC2[i++] = ESCAPE;
            dataBCC2[i++] = ESCAPE3;
        } else{
            dataBCC2[i++] = buf[j]; //aqui nao alteramos
        }
    }

    if(BCC2 == FRAME){
        dataBCC2[i++] = ESCAPE;
        dataBCC2[i++] = ESCAPE2;
    } else if(BCC2 == ESCAPE){
        dataBCC2[i++] = ESCAPE;
        dataBCC2[i++] = ESCAPE3;
    } else{
        dataBCC2[i++] = BCC2; //mais uma vez nao se altera se nao for flag ou escape
    }

    unsigned char *frame = (unsigned char *)malloc(i + 5);
    frame[0] = FRAME;
    frame[1] = TRANSMITER_ADDRESS;

    static unsigned char trama1 = 0;
    unsigned char c = N(trama1);
    frame[2] = c;
    frame[3] = (frame[1] ^ frame[2]);
    memcpy(&frame[4], dataBCC2, i);
    frame[i + 4] = FRAME;

    int frame_size = i + 5;
    free(dataBCC2);
    
    // STUFFING //
    // int current_byte = 4; // nao podemos escrever nos bytes ja ocupados frame[0,1,2,3]
    // for (int i = 0 ; i < bufSize ; i++) 
    // {
    //     if (buf[i] == FRAME || buf[i] == ESCAPE) {
    //         frame_size += 1; 
    //         frame = realloc(frame, frame_size); // aumenta a size da frame em 1 para os escape macros
            
    //         frame[current_byte++] = ESCAPE; 
            
    //         frame[current_byte++] = (buf[i] == FRAME) ? ESCAPE2 : ESCAPE3;
    //     }
    //     else {
    //         frame[current_byte++] = buf[i];
    //     }
    // }

    // frame[current_byte++] = BCC2; 
    // frame[current_byte] = FRAME;

    int nTransmition = 0;

    bool rejected = false; 
    bool accepted = false;

    while(nTransmition < nAttempts) {
         alarmTrigger = FALSE;
         alarm(nTimeout);

        while(alarmTrigger == FALSE && !rejected && !accepted) {
            if(writeBytesSerialPort(frame, frame_size) == -1) return -1;
            
            unsigned char current_byte = 0x00; 
            unsigned char byte_in_c = 0x00;

            state_t state = START_SM;

            while(state != STOP_SM && alarmTrigger == FALSE) {
                if(readByteSerialPort(&current_byte) != -1) {
                    switch (state)
                    {
                    case START_SM:
                        if(current_byte == FRAME) state = FLAG_OK;
                        break;

                    case FLAG_OK:
                        if(current_byte == RECIEVER_ADDRESS) state = A_OK;
                        else if (current_byte != FRAME) state = START_SM;
                    
                    case A_OK:
                        if(current_byte == C_RR0 || current_byte == C_RR1 || current_byte == C_REJ0 || current_byte == C_REJ1 || current_byte == DISCONNECT) {
                            state = C_OK;
                            byte_in_c = current_byte;
                        }

                        else if (current_byte == FRAME) state = FLAG_OK;
                        else state = START_SM;

                    case C_OK:
                        if(current_byte == (RECIEVER_ADDRESS ^  byte_in_c)) {
                            state = BCC_OK;
                        }
                        else if(current_byte == FRAME) state = FLAG_OK;
                        else state = START_SM;
                        break;

                    case BCC_OK:
                        if (current_byte == FRAME) state = STOP_SM;
                        else state = START_SM; 
                        break;

                    default:
                        break;
                    }
                }
            }   // finished checking state machine, already have c value in "byte_in_c"

            if (current_byte == 0) continue;
            else if (current_byte == C_REJ0 || current_byte == C_REJ1) rejected = true;
            else if (current_byte == C_RR0 || current_byte == C_RR1) {
                accepted = true;
                if(trama1 == 0) trama1 = 1;
                else trama1 = 0;
            }
            else continue;
        } 

        if(accepted) break;
        nTransmition++;
    }

    free(frame);
    if(accepted) return frame_size;
    
    llclose(fd);
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet){

    state_t state = START_SM;

    static unsigned char trama1;

    unsigned char rcv;
    unsigned char control;

    int i = 0;

    while(state != STOP_SM){
        if(readByteSerialPort(&rcv) == -1){
            return -1; //deu erro
        }
        else{
            switch(state){
                case START_SM:
                    if(rcv == FRAME) state = FLAG_OK;
                    break;

                case FLAG_OK:
                    if(rcv == TRANSMITER_ADDRESS) state = A_OK;
                    else if (rcv != FRAME) state = START_SM;
                    break;

                case A_OK:
                    if (rcv == (trama1 << 6) || rcv == (trama1 << 6)){
                        state = C_OK;
                        control = rcv;
                    }
                    else if (rcv == FRAME) state = FLAG_OK;
                    else if (rcv == 0x0B){
                        //significa que terminou a conexão
                        unsigned char byte[5];

                        byte[0] = FRAME;
                        byte[1] = RECIEVER_ADDRESS;
                        byte[2] = 0x0B;
                        byte[3] = (byte[1] ^ byte[2]);
                        byte[4] = FRAME;
                        writeBytesSerialPort(byte, sizeof(byte));
                        return 0;
                    }
                    else state = START_SM;
                    break;

                case C_OK:
                    if (rcv == (TRANSMITER_ADDRESS ^ control)) state = DATA;
                    else if (rcv == FRAME) state = FLAG_OK;
                    else state = START_SM;
                    break;

                case DATA: //aqui vamos ler a data passada no frame
                    if (rcv == ESCAPE) state = DATA_ESCAPE;
                    else if (rcv == FRAME){
                        unsigned char bcc2 = packet[i - 1];
                        i--;

                        packet[i] = '\0';
                        unsigned char bcc2_compare = packet[0];

                        for(unsigned int j = 1; j < i; j++){
                            bcc2_compare ^= packet[j];
                        }

                        if(bcc2 == bcc2_compare){
                            state = STOP_SM;
                            unsigned char byte[5];
                            byte[0] = FRAME;
                            byte[1] = RECIEVER_ADDRESS;
                            //0xAA ou 0xAB dependendo do control_C_RX
                            if(trama1 == 0){
                                byte[2] = C_RR0;
                            }
                            else if(trama1 == 1){
                                byte[2] = C_RR1;
                            }
                            byte[3] = (byte[1] ^ byte[2]);
                            byte[4] = FRAME;
                            writeBytesSerialPort(byte, sizeof(byte));

                            if(trama1 == 1){
                                trama1 = 0;
                            }
                            else if(trama1 == 0){
                                trama1 = 1;
                            }
                            return i;
                        }
                        else{
                            //temos informação repetida
                            unsigned char byte[5];
                            byte[0] = FRAME;
                            byte[1] = RECIEVER_ADDRESS;
                            //0x54 ou 0x55 dependendo do control_c_RX
                            if(trama1 == 0){
                                byte[2] = C_REJ0;
                            }
                            else if(trama1 == 1){
                                byte[2] = C_REJ1;
                            }
                            byte[3] = (byte[1] ^ byte[2]);
                            byte[4] = FRAME;
                            writeBytesSerialPort(byte, sizeof(byte));
                            return -1;
                        };
                    }
                    else{
                        packet[i++] = rcv;
                    }
                    break;

                case DATA_ESCAPE:
                    state = DATA;
                    packet[i++] = rcv ^ 0x20;
                    break;
                    
                default:
                    break;
            }
        }
    }

    return -1;
}

int byteDestuffing(unsigned char *frame, int length) {
    int newLength = 0;
    unsigned char destuffedFrame[length];

    for (int i = 0; i < length; i++) {
        if (frame[i] == ESCAPE) {
            destuffedFrame[newLength++] = frame[++i] ^ 0x20;
        } else {
            destuffedFrame[newLength++] = frame[i];
        }
    }
    memcpy(frame, destuffedFrame, newLength);  // Copy back to original frame
    return newLength;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {

    state_t state = START_SM;

    unsigned char rcv;
    
    alarmTrigger = FALSE;

    if(linklayer.role == LlTx){
        (void)signal(SIGALRM, alarmHandler);

        while (nAttempts != 0 && state != STOP_SM){
            unsigned char CLOSE_WORD[5] = {FRAME, TRANSMITER_ADDRESS, DISCONNECT, (TRANSMITER_ADDRESS ^ DISCONNECT) ,FRAME};
            if(writeBytesSerialPort(CLOSE_WORD,5) < 0) return -1;

            alarm(nTimeout);
            while(alarmTrigger == FALSE){
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
                            break;
                    }
                }
            }
            nAttempts--;

            if(state != STOP_SM) return -1;
        }
        unsigned char CLOSE_WORD[5] = {FRAME, RECIEVER_ADDRESS, UA, (RECIEVER_ADDRESS ^ UA),FRAME};
        if(writeBytesSerialPort(CLOSE_WORD,5) < 0){
            return -1;
        }
    }
    else if(linklayer.role == LlRx){
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
                        if (rcv == DISCONNECT) state = C_OK;
                        else if (rcv == FRAME) state = FLAG_OK;
                        else state = START_SM;
                        break;

                    case C_OK:
                        if (rcv == (TRANSMITER_ADDRESS ^ DISCONNECT)) state = BCC_OK;
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
        if(state != STOP_SM) return -1;
        unsigned char CLOSE_WORD[5] = {FRAME, RECIEVER_ADDRESS, DISCONNECT, (RECIEVER_ADDRESS ^ DISCONNECT),FRAME};
        if(writeBytesSerialPort(CLOSE_WORD,5) < 0){
            return -1;
        }
    }
    else{
        return -1; //nao é tx ne rx
    }

    int clstat = closeSerialPort();
    return clstat;
}

/*
int byteStuffing(unsigned char *frame, int length) {
    int newLength = 0;
    unsigned char stuffedFrame[2 * length];  // Allocate enough space for worst case

    for (int i = 0; i < length; i++) {
        if (frame[i] == FRAME || frame[i] == ESCAPE) {
            stuffedFrame[newLength++] = ESCAPE;
            stuffedFrame[newLength++] = frame[i] ^ 0x20;
        } 
        else {
            stuffedFrame[newLength++] = frame[i];
        }
    }
    memcpy(frame, stuffedFrame, newLength);  // Copy back to original frame
    return newLength;
}
*/
