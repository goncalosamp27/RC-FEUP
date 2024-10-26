// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
	// LinkLayer struct creation
	LinkLayer linklayer;
	strcpy(linklayer.serialPort, serialPort);

	if (strcmp(role, "tx") == 0) linklayer.role = LlTx;
    else linklayer.role = LlRx;

	linklayer.baudRate = baudRate;
	linklayer.nRetransmissions = nTries;
	linklayer.timeout = timeout;

	// establish connection
	int fd = llopen(linklayer);

	// error in llopen
	if (fd == -1) { 
        perror("Connection error\n");
        exit(-1);
    }

	switch (linklayer.role) {
		case LlTx: {
			// vamos ter de ler o ficheiro (do PC), enviá-lo e fechar a ligação
			FILE* image = fopen(filename, "rb"); // o rb le byte a byte sem alterar nada no ficheiro

			if(image == NULL) {
				perror("No file was found with the name you provided\n");
				exit(-1);
			}

			int initial_pos = ftell(image);
			fseek(image, 0L, SEEK_END); // mover 0 bytes da posição inicial (em long) e procurar a posição final (SEEK_END)
			int last_pos = ftell(image);

			long int imageSize = last_pos - initial_pos;
			fseek(image, initial_pos, SEEK_SET);

			// start control packet

			int L1 = (int) ceil(log2f((float)imageSize)/8.0);// size of file
			int L2 = strlen(filename);
			unsigned int * size = 5 + L1 + L2; // 5 -> control + TL1 + TL2

			unsigned char *packet_start;
			unsigned int pos = 0;

			packet_start[pos++] = 1;
			packet_start[pos++] = 0;
			packet_start[pos++] = L1;
			long int temp_imgsize = imageSize;

			for (unsigned char i = 0 ; i < L1 ; i++) {
				packet_start[2+L1-i] = temp_imgsize & 0xFF;
				temp_imgsize >>= 8;
				pos++;
    		}
			
			packet_start[pos++] = 1;
			packet_start[pos++] = L2;
			memcpy(packet_start + pos, filename, L2);

			// data packet

			

			// end control packet

			unsigned char *packet_end;
			unsigned int pos2 = 0;
			packet_end[pos2++] = 3; 
			packet_end[pos2++] = 0;
			packet_end[pos2++] = L1;

			long int temp_imgsize2 = imageSize;

			for (unsigned char i = 0 ; i < L1 ; i++) {
				packet_end[2+L1-i] = temp_imgsize2 & 0xFF;
				temp_imgsize2 >>= 8;
				pos2++;
    		}
			
			packet_end[pos2++] = 1;
			packet_end[pos2++] = L2;
			memcpy(packet_end + pos2, filename, L2);

			break;
		}
		
		case LlRx:{
			// vamos ter de ler o ficheiro (da ligação), escrevê-lo e fechar a ligação
		}
		
		default:
			exit(-1);
			break;
	}
}

unsigned char * createControlPack(unsigned int control_field, const char * filename, long int length, unsigned int * size) {

}