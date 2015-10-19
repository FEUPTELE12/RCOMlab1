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
	printf("Reciver - 0\nTransmitter -1\n");
	scanf("%d", &txrx);

	printf("[MAIN] Opening llopen with %d\n", txrx);
	fd = llopen(argv[1], txrx);
	printf("[MAIN] LLOPEN SUCCESFULL\n");

	if(txrx == 1)
	{
		unsigned char buffer[100] = "ola eu sou alguem e tuaa";
		
		llwrite(fd, buffer, 100);
	}
	else
	{
		char buffer[20];
		llread(fd,buffer);
		puts(buffer);
		
	}

	return 1;
}
