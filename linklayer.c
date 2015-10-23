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

#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define FLAG 0x7E
#define ESC 0x7D
#define STFF 0x20
#define SET 0x03
#define DISC 0x0B
#define UA 0x07
#define RR 0x05
#define REJ 0x01
#define A 0x03 //Comando enviado pelo emissor e resposta enviada pelo receptor

//FOR IDENTIFICACION
#define FALSE 0
#define TRUE 1
#define RECEIVER 0
#define TRANSMITTER 1

//TO RECEIVE DATA
#define DATA_RECEIVED 0
#define SET_RECEIVED 1
#define DISC_RECEIVED 2
#define UA_RECEIVED 3
#define RR_RECEIVED 4
#define BAD_RR_RECEIVED 5
#define REJ_RECEIVED 6
#define BAD_REJ_RECEIVED 7

//FOR DEBUGING
#define DEBUG 1

//EXTERNAL VARIABLES
long BAUDRATE = 9600; //38400?
long MAXR = 3; //no maximo de falhas
long MAXT = 1; //valor do temporizador
long MAX_SIZE = 50; //Tamanho maximo de uma frame APÓS stuffing

// FOR ALARM SETUP
int Conts = 0;
int alarm_flag = FALSE;

struct linkLayerStruct{

	char port[20];
	int fd;
	unsigned int sequenceNumber;
	unsigned int timeout;//valor do temporizador
	int frameLength;
	unsigned char* frame;
	unsigned int numTransmissions;
	int State ;//Transmitter Reciver

}linkLayer;

/*
 * Funcion for opening the serial Port
 *
 * Arguments :
 *                  - Char fd -> Port to open
 *
 * Return:
 *									- File Identificator or -1 on error
 *
 */

int config(char *fd) {

	int c, res;
	int i, rs, sum = 0, speed = 0;

	struct termios oldtio,newtio;
	char buf[255];

		/*
	if ( (argc < 2) ||
	 	((strcmp("/dev/ttyS0", argv[1])!=0) &&
	 	(strcmp("/dev/ttyS4", argv[1])!=0) )) {
	 	printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
	 	exit(1);
	 }*/


	/*
	Open serial port device for reading and writing and not as controlling tty
	because we don't want to get killed if linenoise sends CTRL-C.
	*/

	close(*fd);
	rs = open(fd, O_RDWR | O_NOCTTY );
	if (rs <0) {perror(fd); exit(-1); }

	if ( tcgetattr(rs,&oldtio) == -1) { /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	// set input mode (non-canonical, no echo,...)
	newtio.c_lflag = 0;

	newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received*/

	   // VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
	    //leitura do(s) próximo(s) caracter(es)

	tcflush(rs, TCIOFLUSH);

	if ( tcsetattr(rs,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	return rs;

}

/*
 * Funcion thats stuffes a buffer of chars
 *
 * Arguments :
 *								- unsigned char * data: 				  Buffer to Stuff
 *                - unsigned char * stuffed_data :  Buffer with stuffed data
 * 								- int length :										Lenght of Buffer
 *
 * Return:
 *                - length After Stuffing
 *
 */

//TODO fazer verificações
unsigned int byte_stuffing(unsigned char *data, unsigned char *stuffed_data, int length) {
	if(DEBUG) printf("[stuffing] START\n");

	//TODO fazer verificações

	unsigned int i;
	unsigned int j = 0;
	int lengthAfterStuffing = length;

	for(i = 0; i < length; i++) {
		if (data[i] == FLAG || data[i] == ESC) { //Fazemos stuffing em 2 situações: detecção FLAG ou do byte ESC
			stuffed_data[j] = ESC;
			stuffed_data[j+1] = (STFF ^ data[i]); //STFF corresponde a 0x20. Este valor vale 0x5E ou 0x5D conforme a situação
			j++;
			lengthAfterStuffing++;
		}
		else{
			stuffed_data[j] = data[i];
		}
		j++;
	}


	return lengthAfterStuffing;
}

/*
 *
 *
 */

 //TODO Fazer verificações
int sendSupervisionFrame(int fd, unsigned char C) {
	if(DEBUG) printf("[sendSup] START\n");

	int n_bytes = 0;
	unsigned char BBC1;

	if(DEBUG) printf("[sendSup] LinkLayer Sequence number: %d\n" , linkLayer.sequenceNumber);

	if(linkLayer.sequenceNumber == 1){ //se sequenceNumber for 1 o receptor "pede-nos" uma trama do tipo 1
		if(C == RR){
			C = RR | (1 << 7);
		}else if(C == REJ){
			C = REJ | (1 << 7);
		}
	}

	if(DEBUG) printf("Send supervision 0x%x", C);

	BBC1 = A ^ C;

	if(DEBUG) printf("[sendSup] A = %x, C= %x, BCC1 = %x\n", A, C, BBC1);
	linkLayer.frame[0] = FLAG;
	linkLayer.frame[1] = A;
	linkLayer.frame[2] = C;
	linkLayer.frame[3] = BBC1;
	linkLayer.frame[4] = FLAG;

	n_bytes = write(fd, linkLayer.frame, 5);

	if(DEBUG) printf("%x %x %x %x %x\n",linkLayer.frame[0], linkLayer.frame[1], linkLayer.frame[2], linkLayer.frame[3], linkLayer.frame[4]);
	if(DEBUG) printf("[sendSup] Bytes written: %d\n", n_bytes);

	return n_bytes;
}


// TODO VERIFICAÇões
int sendInformationFrame(unsigned char * data, int length) {
	if(DEBUG) printf("\n[SENDIF] START\n[SENDIF]length = %d", length);

	int C = linkLayer.sequenceNumber << 6;
	int BCC1 = A ^ C;
	int i;
	int BCC2 = data[0];

	unsigned char stuffed_data[(MAX_SIZE*2)];
	unsigned char* tmpbuffer = malloc(length +1);

	for(i = 1; i < length; i++){
		BCC2 = (BCC2 ^ data[i]);
	}

	memcpy(tmpbuffer, data, length);
	tmpbuffer[length] = BCC2;

	unsigned int lengthAfterStuffing = byte_stuffing(tmpbuffer, stuffed_data, length+1);

	linkLayer.frame[0] = FLAG;
	linkLayer.frame[1] = A;
	linkLayer.frame[2] = C;
	linkLayer.frame[3] = BCC1;
	memcpy(linkLayer.frame+4, stuffed_data, lengthAfterStuffing);
	linkLayer.frame[4+lengthAfterStuffing] = FLAG;

	if(DEBUG) printf("[SENDINF] length After Stuffing = %d\nSending : ", lengthAfterStuffing);
	if(DEBUG) {
		for(i = 0 ; i < lengthAfterStuffing+5; i++)
			printf("0x%x ", linkLayer.frame[i]);
	}

	if(DEBUG) printf("[SENDINF] END\n");

	linkLayer.sequenceNumber = linkLayer.sequenceNumber ^ 1;
	return (int) write(linkLayer.fd, linkLayer.frame, lengthAfterStuffing+5); // "+5" pois somamos 2FLAG,1BCC,1A,1C
}

int receiveframe(unsigned char *data, int* length) {
	if(DEBUG) printf("[receiveframe] START\n");

	int state = 0; //state machine state
	int stop = FALSE; //control flag
	int Rreturn; //for return
	int Type; //type of frame received
	int num_chars_read = 0; //number of chars read

	unsigned char* charread = malloc(1); //char in serial port
	unsigned char Aread, Cread; //adress and command

	//State machine
	while(stop == FALSE) {

		Rreturn = read(linkLayer.fd, charread, 1); //read 1 char
		if (Rreturn == 1 && DEBUG) printf("[Receiveframe] read -> %x (%d)\n", *charread, Rreturn);
		if (Rreturn < 0) return -1; //nothing

		switch(state) {
			case 0:{ //State Start
				if(DEBUG) printf("[receiveframe] START, char = %x\n", *charread);
				if(*charread == FLAG) state = 1;
				break;
			}

			case 1:{ //Flag -> expect address
				if(*charread == A) {
					if(DEBUG)printf("[receiveframe] ADRESS, char = %x\n", *charread);
					state = 2;
					Aread = *charread;
				} else if(*charread == FLAG) state = 1; //another flag
				break;
			}

			case 2:{ //Address -> Command (many commands possible)
				if(DEBUG) printf("[receiveframe] COMMAND, char = %x ->", *charread);
				Cread = *charread;
				if(*charread == SET)  {
					if(DEBUG) printf("SET\n");
					Type = SET_RECEIVED;
					state = 3;
				}

				else if(*charread == UA) {
					if(DEBUG) printf("UA\n");
					Type = UA_RECEIVED;
					state = 3;
					}

				else if(*charread == DISC) {
					if(DEBUG) printf("DISC\n");
					Type = DISC_RECEIVED;
					state = 3;
				}

				else if(*charread == (RR | ((linkLayer.sequenceNumber) << 7))) {
					if(DEBUG) printf("RR\n");
					Type = RR_RECEIVED;
					state = 3;
				}

				else if(*charread == (RR | ((linkLayer.sequenceNumber^1)  << 7))) { //BAD RR
					if(DEBUG) printf("RR\n");
					Type = BAD_RR_RECEIVED;
					state = 3;
				}

				else if(*charread == (REJ | (linkLayer.sequenceNumber << 7))) {
					if(DEBUG) printf("REJ\n");
					Type = REJ_RECEIVED;
					state = 3;
				}

				else if(*charread == (REJ | ((linkLayer.sequenceNumber^1)  << 7))) {
					if(DEBUG) printf("REJ\n");
					Type = REJ_RECEIVED;
					state = 3;
				}

				else if(*charread == (linkLayer.sequenceNumber << 6)) {
					if(DEBUG) printf("DATA\n");
					Type = DATA_RECEIVED;
					state = 3;
				}

				else if(*charread == FLAG) state = 1;
				else state = 0;
				break;
				}

			case 3:{ //command -> BBC1
				if(DEBUG) printf("[receiveframe] BCC, char = %x\n", *charread);
				if(*charread == FLAG) state = 1;

				else if(*charread != (Aread ^ Cread))
				{
					if(DEBUG) printf("[receiveframe] BCC INCORRECT\n");
					state = 0; //bcc
				}

				else
				{
					if(DEBUG) printf("[receiveframe] BCC CORRECT\n");
					if(Type == DATA_RECEIVED) state = 4;
					else state = 6;
				}
				break;
			}

			case 4:{ //Data expected
				if(DEBUG) printf("[receiveframe] DATA EXPECTED\n");

				if (*charread == FLAG)
					{
						unsigned char BCC2exp = data[0];
						int j;

						for(j = 1;j < num_chars_read - 1; j++)
							BCC2exp = (BCC2exp ^ data[j]);

						if(data[num_chars_read -1] != BCC2exp) {
							sendSupervisionFrame(linkLayer.fd,REJ);
							return -1;
						}
						else
						{

							*length = num_chars_read - 1;
							if(DEBUG) {
								printf("[receiveframe] Lenght: %d Buffer : ", *length);
								puts(data);
							}
							if(DEBUG) printf("[receiveframe] \nSending RR\n");
							linkLayer.sequenceNumber = linkLayer.sequenceNumber ^ 1;
							sendSupervisionFrame(linkLayer.fd, RR);
							free(charread);
							return Type;

						}

					}

					else if(*charread == ESC) state = 5; //deshuffel o proximo

					else
					{
						if(DEBUG) printf("[RECEIVEFRAME] Char to buffer : %c (number: %d)\n",*charread, num_chars_read);
						data[num_chars_read] = *charread;
						num_chars_read++;
						state = 4;
					}

					break;
				}

			case 5:{ //Destuffing

				data[num_chars_read] = *charread ^ STFF;
				num_chars_read++;
				state = 4;
				break;
			}

			case 6:{ //Flag
				if(*charread == FLAG)
				{ //flag
					stop = TRUE;
					state = 0;
				}
				else state = 0;
				break;
			}
			if(DEBUG) printf(",%d]", state);
		}
	}

	if(DEBUG) printf("[receiveframe] STOP -> %d\n", Type);
	free(charread);
	return Type;

}

int write_frame(int fd, unsigned char* buffer, int length, int* i ,int remaning) {


			if(DEBUG) printf("[LLWRITE] Frames = %d\n", *i);

				if(remaning == FALSE)
					sendInformationFrame(buffer + (*i * MAX_SIZE), MAX_SIZE);
				else
					sendInformationFrame(buffer + (*i * MAX_SIZE), length);

			int tmp = receiveframe(NULL,NULL);

			if(DEBUG) printf("[LLWRITE] tmp : %d\n", tmp);

			if(tmp != RR_RECEIVED) {

				if(DEBUG) printf("[LLWRITE] DID NOT RECEIVE RR\n");

				if((tmp == REJ_RECEIVED) || (tmp == BAD_RR_RECEIVED) || (tmp = BAD_REJ_RECEIVED)) {

					if(DEBUG) printf("[LLWRITE] RECEIVED REJ || BAD RR || BAD REJ -> SEND LAST FRAME\n");

					linkLayer.numTransmissions = 0;
					*i--;
				}
				else return -1;
			}
			else if(tmp == RR_RECEIVED) {
				if(DEBUG) printf("[LLWRITE] RECEIVE RR\n");

			}


		if(DEBUG) printf("[LLWRITE] complete Frames = %d\n", *i);
		linkLayer.numTransmissions = 0;
}

void Timeout_SET() {

	if(linkLayer.numTransmissions < MAXR){
		printf("[DEBUG] Timeout %d waiting for UA, re-sending SET\n",  linkLayer.numTransmissions);
		sendSupervisionFrame(linkLayer.fd, SET);
		alarm(linkLayer.timeout);
	}else{
		printf("[DEBUG] Receiver doesnt seem to like me, I give up :(\n");
		exit(-1);
		}
	linkLayer.numTransmissions++;
}

void Timeout_DATA() {
	if(linkLayer.numTransmissions < MAXR ){
			printf("Retrying to send package again\n");
			sendInformationFrame(linkLayer.frame, linkLayer.frameLength); //TODO
			alarm(linkLayer.timeout);
		}
		else{
			printf("Connection timed out. Try again later!\n");
			exit(-1);
		}
		linkLayer.numTransmissions++;

	}


int llopen(char* port, int txrx) {
	if(DEBUG) printf("\n[LLOPEN] START\n");

	int tmpvar;

	linkLayer.fd = config(port);
	linkLayer.sequenceNumber = 0;
	linkLayer.State = txrx;
  linkLayer.numTransmissions = 0;
	linkLayer.timeout = MAXT;
	linkLayer.frame = (unsigned char*) malloc((2*MAX_SIZE)+6);

	if(txrx == TRANSMITTER) {
		if(DEBUG) printf("[llopen] TRANSMITTER\n[llopen] Send SET\n");
		(void) signal(SIGALRM, Timeout_SET);

		sendSupervisionFrame(linkLayer.fd, SET);

		if(DEBUG) printf("[llopen] EXPETING UA\n");
		alarm(linkLayer.timeout);
		tmpvar = receiveframe(NULL,NULL);
		if( tmpvar != UA_RECEIVED ){
			if(DEBUG) printf("[LLOPEN - TRANSMITTER] NOT UA\n");
			return -1; //return error
		}
		else if(tmpvar == UA_RECEIVED) {
			(void) signal(SIGALRM, SIG_IGN);
			if(DEBUG) printf("[LLOPEN - TRANSMITTER] UA RECEIVED\n");
			return linkLayer.fd; //retorn file ID
		}
		return -1;

	}

	else if (txrx == RECEIVER) {
		if(DEBUG) printf("[llopen] RECEIVER\n");
		if(DEBUG) printf("Waiting for SET\n");

		if(receiveframe(NULL,NULL) != SET_RECEIVED)	{
			//TODO falta cenas;
			 if(DEBUG) printf("DID NOT RECEIVE SET");
			return -1;
		}
		sendSupervisionFrame(linkLayer.fd,UA);
		if(DEBUG) printf("UA sent\n");

		return linkLayer.fd;
	}

	return 0;
}

int llread(int fd,unsigned char* buffer) {
	if(DEBUG) printf("[LLREAD] START\n");

	int num_chars_read = 0, aux_num_chars = 0;
	int Type;
	int i = 0;

	unsigned char aux[MAX_SIZE];

	linkLayer.sequenceNumber = 0;

	do {
		Type= receiveframe(aux, &aux_num_chars);

		if(DEBUG) printf("aux = %s, aux_num_chars = %d\n",aux, aux_num_chars);

		if(Type == DATA_RECEIVED)
		{
			memcpy(buffer + (i * MAX_SIZE), aux, aux_num_chars);
			i++;
			num_chars_read += aux_num_chars;
			aux_num_chars = 0;
		}
		else if(Type == DISC_RECEIVED) {
			llclose(linkLayer.fd);
			return num_chars_read;
		}
	} while(1);

	return -1;
}

int llwrite(int fd, unsigned char* buffer, int length) {
    if(DEBUG) printf("\n[LLWRITE] START\n");

	int CompleteFrames =  length / (MAX_SIZE);
	int remainingBytes =  length % (MAX_SIZE);
	int flag = 1;
	int i;

	if(DEBUG) printf("[LLWRITE] lenght = %d, complete Frames = %d , remaining bytes = %d\n", length, CompleteFrames, remainingBytes);

	//(void) signal(SIGALRM, Timeout_RR);

	linkLayer.timeout = 0;
	linkLayer.numTransmissions = 0;
	linkLayer.timeout = MAXT;
	linkLayer.sequenceNumber = 0;


	for(i = 0; i < CompleteFrames; i++) write_frame(fd, buffer, 0, &i , FALSE);
	if(remainingBytes > 0) write_frame(fd, buffer, remainingBytes, &i, TRUE);


	llclose(linkLayer.fd);
	if(DEBUG) printf("[LLWRITE] END\n");
	return 0;

}

int llclose(int fd) {
	if(DEBUG) printf("[LLCLOSE] START\n");
	int tmp;

	sendSupervisionFrame(fd, DISC);
	tmp = receiveframe(NULL,NULL);

	if(tmp == DISC_RECEIVED)	{
		if(DEBUG) printf("[LLCLOSE] Expecting UA\n");
		sendSupervisionFrame(fd, UA);
		close(fd);
		return 1;
	}

	else if(tmp == UA_RECEIVED)	{
		if(DEBUG) printf("[LLCLOSE] EXITING AS RECEIVER\n");
		close(fd);
		return 1;
	}

	else {
		if(DEBUG) printf("[LLCLOSE] PROBLEMS\n");
		return 1;
	}
}
