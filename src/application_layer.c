// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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