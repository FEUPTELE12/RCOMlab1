#include "linklayer.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define TRANSMITTER 1;

int main (int argc, char** argv) {
	int txrx;
	int fd;
	FILE * pFile;
	char * buffer;
	size_t result;
	long lSize;
	
	printf("Reciver - 0\nTransmitter -1\n");
	scanf("%d", &txrx);

	printf("[MAIN] Opening llopen with %d\n", txrx);
	fd = llopen(argv[1], txrx);
	printf("[MAIN] LLOPEN SUCCESFULL\n");

	if(txrx == 1)
	{
		pFile = fopen("penguin.gif", "r");
		
		// obtain file size:
		//fseek (pFile , 0 , SEEK_END);
		//lSize = ftell (pFile);
		//rewind (pFile);
		//printf("lsize = %ld", lSize);
		buffer = (char*) malloc (sizeof(char)*10968);
		//if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}
			
		//result = fread (buffer,sizeof(char),lSize,pFile);
		//if (result != lSize) {fputs ("Reading error",stderr); exit (3);}
		
		llwrite(fd, buffer, lSize);
		fclose (pFile);
		free (buffer);
	}
	else
	{
		
		llread(fd,buffer);
		
		
	}

	return 1;
}
