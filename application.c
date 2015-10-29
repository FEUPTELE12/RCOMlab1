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

#define APP_START 0x02
#define APP_END 0x03
#define C_DATA 0x01
#define T_SIZE 0x00 
#define APP_MAX_SIZE 1024
#define TRANSMITTER 1

#define DEBUG 1


struct applicationLayerstruct{

	int fileDescriptor;
	int status;
	unsigned char sequenceNumber;
	unsigned char package[APP_MAX_SIZE];

}applicationLayer;

int sendAppControlPackage(unsigned char C, int fileSize, unsigned char L){

	int i;
	if(DEBUG)printf("[APP] Sending Control Package.\n");
	
	applicationLayer.package[0] = C;
	applicationLayer.package[1] = T_SIZE;
	applicationLayer.package[2] = L;
	applicationLayer.package[3] = fileSize >> 8;
	applicationLayer.package[4] = fileSize & 0xFF;
	
	//NOTA: É assim para o caso de L=2, fazer função geral que funcione com todos os L
	
	if(DEBUG)
		printf("[APP] Control Frame Sent: %x; %x; %x; %x; %x; \n",applicationLayer.package[0],applicationLayer.package[1],applicationLayer.package[2],applicationLayer.package[3],applicationLayer.package[4]);

	if(DEBUG)printf("[APP] Starting llwrite. \n");
	
	return (int) llwrite(applicationLayer.fileDescriptor, applicationLayer.package, L+3);
}

int sendAppDataPackage(unsigned char * data, int length){

	if(length > APP_MAX_SIZE){
		printf("[APP] length > APP_MAX\n");
		return -1;
	}
	unsigned char L[2];

	L[0] = length & 0xFF;
	L[1] = length >> 8;

	applicationLayer.package[0] = C_DATA;
	applicationLayer.package[1] = applicationLayer.sequenceNumber;
	applicationLayer.package[2] = L[1];
	applicationLayer.package[3] = L[0];
	memcpy(applicationLayer.package+4, data, length);

	if(applicationLayer.sequenceNumber == 0xFF)
		applicationLayer.sequenceNumber = 0x00;
	else
		applicationLayer.sequenceNumber++;

	return (int) llwrite(applicationLayer.fileDescriptor, applicationLayer.package, length+4);
}

int receiveControlPackage(void){
	
	if(DEBUG)printf("[APP] Receiving Control Frame.\n");
	
	unsigned char nBytes;
	int size;
	int i;
	
	unsigned char* buffer = (unsigned char*) malloc (sizeof(unsigned char) * 5);
		
	llread(applicationLayer.fileDescriptor ,buffer);
	
	if(DEBUG)
		printf("[APP] Control Frame Received: %x; %x; %x; %x; %x; \n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
	
	
	if(	(buffer[0] != APP_START) && (buffer[0] != APP_END) ){
		if(DEBUG)printf("\n[APP] Unexpected Control Byte.\n");
		return -1;
	}
	
	if(buffer[1] != T_SIZE){
		if(DEBUG)printf("\n[APP] Unexpected Type Byte. \n");
		return -1;
	}
	
	nBytes = buffer[2];
	
	/*for (i = 0; i != nBytes; ++i) {
		size += buffer[i+3] << (8*nBytes - i * 8); 
	}*/
	
	size = 256*buffer[3]+buffer[4];
	
	return size;
}

int receiveDataPackage(unsigned char *data){
	
	
	int i;
	int nDataBytes = 0;
	int K;
	
	llread(applicationLayer.fileDescriptor, applicationLayer.package);
	
	//Extract the data from the app_frame
	//for(i=0; i < (completePackages+1); i++){
		
		
		if(applicationLayer.package[0] != C_DATA){
			if(DEBUG)printf("\n[APP] C_DATA not received. \n");
			return -1;
		}
		
		if(DEBUG){
			if(applicationLayer.package[1] != applicationLayer.sequenceNumber)
				printf("\n[APP] Unexpected sequence number\n");
		}
		
		K = 256*applicationLayer.package[2]+applicationLayer.package[3];
		
		
		memcpy(data, applicationLayer.package+4, K);
		
	return K;
}

int setting(char *filename)
{
	char buffer[256];
	int i;
	FILE* fd = fopen("settings.txt", "r");
	if(fd == NULL){
		printf("Problems finding settings.txt, include in the folder\n");
		return -1;
	}
	
	for(i = 0; i < 26; i++) fscanf(fd, "%s", buffer);
	BAUDRATE = atoi(buffer);
	for(i = 0; i < 4; i++) fscanf(fd, "%s", buffer);
	MAX_SIZE = atoi(buffer);
	for(i = 0; i < 3; i++) fscanf(fd, "%s", buffer);
	MAXT = atoi(buffer);
	for(i = 0; i < 5; i++) fscanf(fd, "%s", buffer);
	MAXR = atoi(buffer);
	for(i = 0; i < 5; i++) fscanf(fd, "%s", buffer);
	memcpy(filename, buffer, strlen(buffer));
	return 1;
}
	
int main (int argc, char** argv) {
	int txrx;
	int fd;
	int i;
	char filename[30];
	FILE *pFile;
	int lSize;
	unsigned char L = 0;
	if(!setting(filename)) return -1;
	//USER INTERFACE
	printf("Receiver - 0\nTransmitter -1\n");
	scanf("%d", &txrx);
	
	applicationLayer.status = txrx;
	
	write(2, "\n[APP] Opening Port, Stand by", sizeof("\n[APP] Opening Port, Stand by"));
	applicationLayer.fileDescriptor = llopen(argv[1], applicationLayer.status); //opening port
	printf("\n[APP] Port Open");


	if(txrx == 1)  //TRANSMITTER 
	{
		
		fseek(pFile, 0, SEEK_END);
		lSize = ftell(pFile);
		fseek(pFile,0, SEEK_SET);
	
		int aux = lSize;

		while(aux != 0){
			aux = aux	>>	8;
			L++;
		}
		
		if(DEBUG)printf("[APP- Transmitter] nº of Bytes of lSize: %d\n",L);
		
		
		int completePackages = lSize/ APP_MAX_SIZE;
		int remainingBytes = lSize % APP_MAX_SIZE;

		if(DEBUG)printf("\n[APP] Expecting to SEND %d Complete Frams.\n" , completePackages);
			if(DEBUG)printf("\n[APP] Expecting to send %d remaining bytes.\n" , remainingBytes);
		
		unsigned char* buffer = (unsigned char*) malloc (sizeof(unsigned char) * lSize);
		
		
		pFile = fopen(filename, "rb");
		fread(buffer,sizeof(unsigned char), lSize, pFile);

		sendAppControlPackage(APP_START, lSize,L);//START
		applicationLayer.fileDescriptor = llopen(argv[1], applicationLayer.status);
	
		//Sending Data
		for (i = 0; i < completePackages; i++)
		{
				printf("\n[APP] Sending frame number: %d \n" , i);
			
			
				printf("sending: %d\n", sendAppDataPackage(buffer+i*APP_MAX_SIZE, APP_MAX_SIZE));

				//llclose é chamado sempre que fazemos um llwrite logo temos de voltar a fazer llopen.
				applicationLayer.fileDescriptor = llopen(argv[1], applicationLayer.status);
		}
	
		if(DEBUG)printf("\n\n[APP] Sending the remaining bytes %d...\n", remainingBytes);
	
		sendAppDataPackage(buffer+i*APP_MAX_SIZE, remainingBytes);
		applicationLayer.fileDescriptor = llopen(argv[1], applicationLayer.status);

		sendAppControlPackage(APP_END, lSize,L);//END

		//llclose(fd); //Closing Connection
	}
	//END TRANSMITER
	
	else  //RECEIVER
	{
		//Devemos assumir que o receptor não faz ideia de quanto é o tamanho do ficheiro
		int rFileSize;
		int dataBytesReceived = 0;
		unsigned char* buffer;
		unsigned char* data;
		applicationLayer.sequenceNumber = 0;
		
		rFileSize = receiveControlPackage();
		
		if(rFileSize == -1){
				printf("[APP] Received -1 from receiveControlFrame, exiting.\n");
				exit -1;
		}
		
		applicationLayer.fileDescriptor = llopen(argv[1], applicationLayer.status);
		
		buffer = (unsigned char*) malloc (sizeof(unsigned char) * rFileSize);
		
		int completePackages = rFileSize/ APP_MAX_SIZE;
		int remainingBytes = rFileSize % APP_MAX_SIZE;

		if(DEBUG)printf("\n[APP] Expecting to RECEIVE %d Complete Packages.\n" , completePackages);
		if(DEBUG)printf("\n[APP] Expecting to RECEIVE %d remaining bytes.\n" , remainingBytes);

		
		data = (unsigned char*) malloc (sizeof(unsigned char) * rFileSize);
		
		//Receiving Data
		for (i = 0; i <= completePackages; i++)  // "<=" devido aos remaining bytes
		{
			
				if(i == completePackages){
					if(DEBUG)
					printf("\n\n[APP] Receiving the remaining bytes...\n");
				}else
					printf("\n[APP] Received Package number: %d \n" , i);
					
			
				dataBytesReceived += receiveDataPackage(data+i*APP_MAX_SIZE);
				applicationLayer.fileDescriptor = llopen(argv[1], applicationLayer.status);
				
				if(applicationLayer.sequenceNumber == 0xFF)
					applicationLayer.sequenceNumber = 0x00;
				else
					applicationLayer.sequenceNumber++;
		}
		
		printf("\n[APP] Expected number of bytes: %d .\n" , rFileSize);
		printf("\n[APP] Number of bytes received: %d .\n" , dataBytesReceived);
		
		
		if(	dataBytesReceived != rFileSize)
			printf("\n[APP] Unexpected number of Data Bytes.\n");
		else
			printf("\n[APP] The expected number of bytes was received. \n");
	
		if(receiveControlPackage() != rFileSize)
			printf("\n[APP] rFileSize received on the END frame doesn't have the correct value\n");
	
		fwrite( data, sizeof(unsigned char), dataBytesReceived, pFile);

		}
		//END RECEIVER
		
	return 1;
}

