// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int showstats = 0;


char log2aux(int number) {
    char result = 0x00;   
    if (number == 0) return -1;  
    while (number >>= 1) result++;
    return result;     
}

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
	// LinkLayer struct creation
	LinkLayer linklayer;
	strcpy(linklayer.serialPort, serialPort);

	if (strcmp(role, "tx") == 0) linklayer.role = LlTx;
    else if (strcmp(role, "rx") == 0) linklayer.role = LlRx;
	else exit(-1);

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

			printf("\nFile successfully found\n");

			// variables for both end and start control packets
			long int initial_pos = ftell(image); // da nos a pos inicial
			fseek(image, 0L, SEEK_END); // mover 0 bytes da posição inicial (em long) e procurar a posição final (SEEK_END)
			long int last_pos = ftell(image); // da nos a pos final

			long int imageSize = last_pos - initial_pos;
			fseek(image, initial_pos, SEEK_SET);  // volta a pos inicial

			int L1 = (int)(log2aux(imageSize) /8) + 1;// size of file
			int L2 = strlen(filename);
			unsigned int size = 5 + L1 + L2; // 5 -> control + bT1 + bL1 + bT1 + bL2... resto é dos V1 e V2 (valores de L1 e L2)

			// start control packet
			unsigned char *packet_start = (unsigned char*) malloc(size);
			unsigned int pos = 0;

			packet_start[pos++] = 1; // values 1 -> start
			packet_start[pos++] = 0; // 0 -> file size
			packet_start[pos++] = L1; // l1 -> tem o size do file
			
			long int temp_imgsize = imageSize;
			for (unsigned char i = 0 ; i < L1 ; i++) { // colocar os valores do tamanho dos bytes no packet
				packet_start[2+L1-i] = temp_imgsize & 0xFF; // começamos pelo final e andamos para tras
				temp_imgsize = temp_imgsize >> 8;
				pos++;
    		} 
			
			packet_start[pos++] = 1; // 1 -> file name
			packet_start[pos++] = L2; // len da string
			memcpy(packet_start + pos, filename, L2); // colocar o nome do ficheiro no packet

			if(llwrite(packet_start, size) == -1){
				exit(-1);
			}

			// data packet
			unsigned char sequence_value = 0; // between 0 -> 99
			unsigned char *image_to_bytes;
			image_to_bytes = (unsigned char *)malloc(sizeof(unsigned char) * imageSize);

			size_t bytesRead = fread(image_to_bytes, sizeof(unsigned char), imageSize, image);
			printf("Bytes Read (from image file): %d", (int)bytesRead);

			if (bytesRead != imageSize) {
				perror("Error reading image");
				free(image_to_bytes);
				exit(-1);
			}
			printf("\nImage successfully read");

			long int number_of_bytes_to_write = imageSize;

			int count = 1;
			while(number_of_bytes_to_write > 0) {
				int size_of_data_field;

				if (number_of_bytes_to_write > MAX_PAYLOAD_SIZE) size_of_data_field = MAX_PAYLOAD_SIZE;
				else size_of_data_field = number_of_bytes_to_write; 

				unsigned char * packet_data_field = (unsigned char *)malloc(size_of_data_field);
				memcpy(packet_data_field, image_to_bytes, size_of_data_field);

				// now we need to create the data packet //
				unsigned char L2_2 = (size_of_data_field >> 8) & 0xFF;  // Higher byte
    			unsigned char L1_1 = size_of_data_field & 0xFF;   

				unsigned char * data_packet = (unsigned char *) malloc (size_of_data_field + 4);
				data_packet[0] = 2;
				data_packet[1] = sequence_value;
				data_packet[2] = L2_2;
				data_packet[3] = L1_1;
    			memcpy(data_packet + 4, packet_data_field, size_of_data_field);

				if(llwrite(data_packet, (size_of_data_field + 4)) == -1) {
                    printf("Error writing data packets\n");
                    exit(-1);
                }
				printf("Data Packet %d created", count);
				count++;

				sequence_value = (sequence_value + 1) % 100;
				image_to_bytes += size_of_data_field;
				number_of_bytes_to_write -= size_of_data_field;
				
				free(data_packet);
				free(packet_data_field);
			}
			//free(image_to_bytes);

			// end control packet
			unsigned char *packet_end = (unsigned char*)malloc(size);
			unsigned int pos2 = 0;

			packet_end[pos2++] = 3; // values 3 -> end
			packet_end[pos2++] = 0;
			packet_end[pos2++] = L1;

			long int temp_imgsize2 = imageSize;

			for (unsigned char i = 0 ; i < L1 ; i++) {
				packet_end[2+L1-i] = temp_imgsize2 & 0xFF;
				temp_imgsize2 = temp_imgsize2 >> 8;
				pos2++;
    		}
			
			packet_end[pos2++] = 1;
			packet_end[pos2++] = L2;
			memcpy(packet_end + pos2, filename, L2);

			if(llwrite(packet_end, size) == -1) {
				exit(-1);
			}

			if(llclose(showstats) == -1) exit(-1);
			break;
		}
		
		case LlRx:{
			// vamos ter de ler o ficheiro (da ligação), escrevê-lo e fechar a ligação
			FILE *newFile = fopen(filename, "wb"); //write back
			int i = 1;
			while(TRUE){
				unsigned char *p = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
				int bytes = llread(p);
				if(bytes > 0){
					unsigned char first = p[0];
					if(first == 1 || first == 3){ //é control packet (ve powerpoint)
					 	if (first == 1) printf("\nstarted reading start control packets\n");
						if (first == 3) printf("\nstarted reading end control packets\n");
						unsigned char length = p[2];
						int size = 0;

						for(int i = 0; i < length; i++){
							size = size << 8;
							size = size | p[3+i];
						}

						unsigned char nameLength = p[3 + length]; //tamanho do nome

						char *new = (char *)malloc(nameLength + 1); //alocar mem para o nome

						memcpy(new, p + 4 + length, nameLength);
						new[nameLength] = '\0'; //terminador

						if (first == 1) {
            				printf("Finished recieving start control packets: %s (%d bytes)\n", new, size);
        				} else if (first == 3) {
            				printf("Finished recieving end control packets: %s (%d bytes)\n", new, size);
            				free(new);  // Libertar memória antes de sair
            				break;
        				}
					}
					else if(first == 2){ //é data packet
						int L2_2 = p[2] << 8;  // Higher byte
    					int L1_1 = p[3];
						int sum = L2_2 + L1_1;
						printf("\nReading Data Packet number: %d\n", i);
						i++;
						fwrite(p + 4, sizeof(unsigned char), sum, newFile);
					}
				}
				free(p);
			}
			fclose(newFile);
			llclose(1);
		}
		
		default:
			exit(-1);
			break;
	}
}
