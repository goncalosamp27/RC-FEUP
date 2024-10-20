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
		}
			break;
		
		case LlRx:{
			// vamos ter de ler o ficheiro (da ligação), escrevê-lo e fechar a ligação
		}
		
		default:
			exit(-1);
	}
}
