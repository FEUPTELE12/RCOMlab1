#include "linklayer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TRANSMITTER 1;

int main (int argc, char** argv) {
	int txrx;
	int fd;
	FILE *pFile;

	long lSize = 10968;
	//long lSize = 256;

	printf("Reciver - 0\nTransmitter -1\n");
	scanf("%d", &txrx);

	printf("[MAIN] Opening llopen with %d\n", txrx);
	fd = llopen(argv[1], txrx);
	//printf("[MAIN] LLOPEN SUCCESFULL\n");

	if(txrx == 1)
	{
		unsigned char* buffer = (unsigned char*) malloc (sizeof(unsigned char) * lSize);
		//pFile = fopen("test.txt", "r" );
		//pFile = fopen("test_file.dat", "rb");
		pFile = fopen("penguin.gif", "rb");
		fread(buffer,sizeof(unsigned char), lSize, pFile);
		llwrite(fd, buffer, lSize);
		printf("transmited = %ld, received = %ld, timeout times= %ld, REJ received = %ld\n",transmited,received,timeout_time, rej_send_received);
	}
	else
	{
		unsigned char* buffer = (unsigned char*) malloc (sizeof(unsigned char) * lSize);
		//pFile = fopen("test_clone.txt", "w");
		//pFile = fopen("test_file_clone.dat", "wb");
	 pFile = fopen("penguin_clone.gif", "wb");
		llread(fd,buffer);
		fwrite( buffer,sizeof(unsigned char), lSize, pFile);
		//printf("Buffer read: %s\n", buffer);

		printf("transmited = %ld, received = %ld, timeout times= %ld, REJ sent = %ld\n",transmited,received,timeout_time, rej_send_received);
	}

	return 1;
}
