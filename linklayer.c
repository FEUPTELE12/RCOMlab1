/*RS 232 DATALINK SOURCE*/

/* AUTHORS :
 *   				-Afonso Ferreira Pinto Gomes Moreira : 201205007
 *					-Catarina Mendes Cruz:                 201207054
 *          -José Delfim Ribeiro Valverde :        201205119
 */


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
#include <errno.h>

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
#define BAD_BBC_RECEIVED 8
#define BAD_DATA_RECEIVED 9

//FOR DEBUGING
#define DEBUG 1

//EXTERNAL VARIABLES
long BAUDRATE = 9600; //38400?
long MAXR = 3; 		  	//Maximum number atents
long MAXT = 2;    		// Timeout value in seconds
long MAX_SIZE = 50;   //Maximum data to send in a frame
long transmited = 0;
long received = 0;
long timeout_time = 0;
long rej_send_received;
// FOR ALARM SETUP
#define DATA 0x00

struct linkLayerStruct{ //IMPORTANT INFORMACION

	char port[20]; 							   // port name
	int fd; 										   // port identifier
	unsigned int sequenceNumber;   // 1 or 0
	unsigned int timeout;					 // Timeout value in seconds
	int frameLength; 							 // Size of the frame
	unsigned char* frame; 				 // Array of contents in the framer (just for informacion)
	unsigned int numTransmissions; // Maximum number of retries
	int State ;										 // Transmitter/Reciver
	unsigned char alarm_char;			 // Command char for alarm

}linkLayer;

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

unsigned int byte_stuffing(unsigned char *data, unsigned char *stuffed_data, int length) {
	if(DEBUG) printf("[stuffing] START\n");

 if(data == NULL || stuffed_data == NULL || length <= 0)
 	return -1;

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

int sendSupervisionFrame(int fd, unsigned char C) {
	if(DEBUG) printf("[sendSup] START\n");

	int n_bytes = 0;
	unsigned char BBC1 = A ^ C;;
  unsigned char* frame = malloc(5);

	if(frame == NULL)
		return -1;

	if(DEBUG){
		printf("[sendSup] LinkLayer Sequence number: %d\n" , linkLayer.sequenceNumber);
		printf("[sendSup]Send supervision 0x%x", C);
		printf("[sendSup] A = %x, C= %x, BCC1 = %x\n", A, C, BBC1);
	}
	if(linkLayer.sequenceNumber == 1){ //se sequenceNumber for 1 o receptor "pede-nos" uma trama do tipo 1
		if(C == RR){
			C = RR | (1 << 7);
		}else if(C == REJ){
			C = REJ | (1 << 7);
		}
	}

	//constructing frame
	frame[0] = FLAG;
	frame[1] = A;
	frame[2] = C;
	frame[3] = BBC1;
	frame[4] = FLAG;

	//writing frame to serial port
	transmited++;
	n_bytes = write(fd,frame, 5);
	if(n_bytes != 5)
		return -1;

	if(DEBUG){
		printf("%x %x %x %x %x\n",frame[0], frame[1], frame[2], frame[3], frame[4]);
		printf("[sendSup] Bytes written: %d\n", n_bytes);
  }

	return n_bytes;
}

int sendInformationFrame(unsigned char * data, int length) {
	if(DEBUG) printf("\n[SENDIF] START\n[SENDIF]length = %d", length);

	//verificacions
	if(data == NULL &&length <= 0)
		return -1;

	int C = linkLayer.sequenceNumber << 6; //Comand is only sequence number on the 6 bit
	int BCC1 = A ^ C; //BCC1 xor adress and Command
	int BCC2 = data[0]; //BCC2 xor data values
	unsigned int lengthAfterStuffing;
	int i;

	unsigned char* frame = malloc(length*2+6);
	unsigned char stuffed_data[(MAX_SIZE*2)];
	unsigned char* tmpbuffer = malloc(length +1);
	if(tmpbuffer == NULL || frame == NULL)
		return ENOMEM; //memory error;

	for(i = 1; i < length; i++)	BCC2 = (BCC2 ^ data[i]);
	memcpy(tmpbuffer, data, length);
	tmpbuffer[length] = BCC2;

	lengthAfterStuffing = byte_stuffing(tmpbuffer, stuffed_data, length+1);
	if(lengthAfterStuffing < length)
		return -1; //error in stuffing

	//Making Frame
	frame[0] = FLAG;
	frame[1] = A;
	frame[2] = C;
	frame[3] = BCC1;
	memcpy(frame+4, stuffed_data, lengthAfterStuffing);
	frame[4+lengthAfterStuffing] = FLAG;

	if(DEBUG){
		printf("[SENDINF] length After Stuffing = %d\nSending : ", lengthAfterStuffing);
		for(i = 0 ; i < lengthAfterStuffing+5; i++)
			printf("0x%x ",frame[i]);
		printf("[SENDINF] END\n");
  }
	transmited++;
	return (int) write(linkLayer.fd, frame, lengthAfterStuffing+5); // "+5" pois somamos 2FLAG,1BCC,1A,1C
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
	received++;
	//State machine
	while(stop == FALSE) {

		Rreturn = read(linkLayer.fd, charread, 1); //keeps waiting for 1 char
		if (Rreturn == 1 && DEBUG) printf("[Receiveframe] (state = %d) read -> %x (%d)\n",state, *charread, Rreturn);
		if (Rreturn < 0) return -1; //nothing


		switch(state) {
			case 0:{ //State Start
				if(DEBUG) printf("[receiveframe] START, char = %x\n", *charread);
				if(*charread == FLAG) state = 1;
				break;
			}

			case 1:{ //Read Flag -> Expecting address
				if(*charread == A) {
					if(DEBUG)printf("[receiveframe] ADRESS, char = %x\n", *charread);
					state = 2;
					Aread = *charread;
				} else if(*charread == FLAG) state = 1; //another flag
				break;
			}

			case 2:{ //Read Address -> Expecting Command
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
					if(DEBUG) printf("BAD RR\n");
					Type = BAD_RR_RECEIVED;
					state = 3;
				}

				else if(*charread == (REJ | (linkLayer.sequenceNumber << 7))) {
					if(DEBUG) printf("BAD REJ\n");
					Type = BAD_REJ_RECEIVED;
					state = 3;
				}

				else if(*charread == (REJ | ((linkLayer.sequenceNumber^1)  << 7))) {
					if(DEBUG) printf("REJ\n");
					Type = REJ_RECEIVED;
					state = 3;
				}
				//If I0 fails we must receive a REJ0 and send I0 again!

				else if(*charread == ((linkLayer.sequenceNumber^1) << 6)) {
					if(DEBUG) printf("DATA ERROR\n");
					Type = BAD_DATA_RECEIVED;
					state = 3;
				}
				else if(*charread == (linkLayer.sequenceNumber << 6)) {
					if(DEBUG) printf("DATA\n");
					Type = DATA_RECEIVED;
					state = 3;
				}
				else if(*charread == FLAG) state = 1; //error
				else state = 0; //error
				break;
				}

			case 3:{ //Read Command -> Expecting BBC1
				if(DEBUG) printf("[receiveframe] BCC, char = %x\n", *charread);
				if(*charread == FLAG) state = 1;

				else if(*charread != (Aread ^ Cread))
				{
					if(DEBUG) printf("[Receiveframe] BCC INCORRECT\n");
					if(Type == DATA) sendSupervisionFrame(linkLayer.fd,REJ);
					return BAD_BBC_RECEIVED;
					state = 0; //bcc
				}

				else
				{
					if(DEBUG) printf("[receiveframe] BCC CORRECT\n");
					if(Type == DATA_RECEIVED || Type == BAD_DATA_RECEIVED) state = 4; //tipo...
					else state = 6;
				}
				break;
			}

			case 4:{ //BCC1 correct -> Data expected
				if(DEBUG) printf("[receiveframe] DATA EXPECTED\n");

				if (*charread == FLAG)
					{
						unsigned char BCC2exp = data[0];
						int j;

						for(j = 1;j < num_chars_read - 1; j++)
							BCC2exp = (BCC2exp ^ data[j]);

						if(data[num_chars_read -1] != BCC2exp) {
							return BAD_BBC_RECEIVED;
						}
						else //BCC2 Correct
						{
							*length = num_chars_read - 1;

							if(DEBUG) {
								printf("[receiveframe] Lenght: %d Buffer : ", *length);
								puts(data);
								printf("[receiveframe] \nSending RR\n");
							}

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

void Timeout() {

	if(linkLayer.numTransmissions < MAXR && linkLayer.numTransmissions != 0){
		printf("[TIMEOUT] Timeout %d\n",  linkLayer.numTransmissions+1);
		if(linkLayer.alarm_char != DATA)
			if(!sendSupervisionFrame(linkLayer.fd, linkLayer.alarm_char)) exit(-1);
		else
			if(!sendInformationFrame(linkLayer.frame, linkLayer.frameLength)) exit(-1);
		alarm(linkLayer.timeout);
	}
	else if (linkLayer.numTransmissions == MAXR) {
		printf("[TIMEOUT] Port closing, didn't get any response\n");
		close(linkLayer.fd);
		exit(-1);
	}
	else if(linkLayer.numTransmissions == 0){
		 alarm(linkLayer.timeout); //Dont send Anything on first atent
		 printf("[TIMEOUT] Timeout %d\n",  linkLayer.numTransmissions+1);
	 }
	linkLayer.numTransmissions++;
	timeout_time++; //for external variable
}

int write_frame(int fd, unsigned char* buffer, int length, int* i ,int remaning) {

	(void) signal(SIGALRM, Timeout);
	if(DEBUG) printf("[LLWRITE] Frame = %d || Sequence Number = %d\n", *i, linkLayer.sequenceNumber);
	printf(".");

	//SENDING FRAME
	if(remaning == FALSE) {
		linkLayer.frameLength = MAX_SIZE;
		memcpy(linkLayer.frame, buffer + (*i * MAX_SIZE), MAX_SIZE); //to be used by the timeout
		sendInformationFrame(buffer + (*i * MAX_SIZE), MAX_SIZE);
	}
	else {
		linkLayer.frameLength = length;
		memcpy(linkLayer.frame, buffer + (*i * MAX_SIZE), length);
		sendInformationFrame(buffer + (*i * MAX_SIZE), length);
  }

	linkLayer.sequenceNumber = linkLayer.sequenceNumber ^ 1; //after sending frame switch sequence Number
	alarm(linkLayer.timeout);
	int tmp = receiveframe(NULL,NULL);

	if(DEBUG) printf("[LLWRITE] tmp : %d\n", tmp);

	if(tmp != RR_RECEIVED) {
		*i = *i - 1;
		(void) signal(SIGALRM, SIG_IGN);
		if(DEBUG) printf("[LLWRITE] DID NOT RECEIVE RR\n");

		if( (tmp == BAD_RR_RECEIVED) || (tmp = BAD_REJ_RECEIVED)) {

			if(DEBUG) printf("[LLWRITE]BAD RR || BAD REJ (%d)-> SEND LAST FRAME\n", tmp);

			rej_send_received++;
		}
		else if(tmp == REJ_RECEIVED) rej_send_received++;
		else return -1;
	}
	else if(tmp == RR_RECEIVED) {
		(void) signal(SIGALRM, SIG_IGN);
		if(DEBUG) printf("[LLWRITE] RECEIVE RR\n");

	}


	if(DEBUG) printf("[LLWRITE] complete Frames = %d\n", *i);
	linkLayer.numTransmissions = 0;
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
	linkLayer.alarm_char = SET;

	printf("[RS-232] Porto open - Starting");

	if(txrx == TRANSMITTER) {
		linkLayer.State = TRANSMITTER;
		if(DEBUG) printf("[llopen] TRANSMITTER\n[llopen] Send SET\n");
		printf(" transmission, stand by ");
		(void) signal(SIGALRM, Timeout);

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
		linkLayer.State = RECEIVER;
		if(DEBUG) printf("[llopen] RECEIVER\n");
		if(DEBUG) printf("Waiting for SET\n");
		printf(" recepcion, stand by ");

		if(receiveframe(NULL,NULL) != SET_RECEIVED)	{
			//TODO falta cenas;
			 if(DEBUG) printf("DID NOT RECEIVE SET");
			return -1;
		}
		sendSupervisionFrame(linkLayer.fd,UA);
		if(DEBUG) printf("UA sent\n");

		return linkLayer.fd;
	}

	return 1;
}

int llread(int fd,unsigned char* buffer) {
	if(DEBUG) printf("[LLREAD] START\n");

	int num_chars_read = 0, aux_num_chars = 0;
	int Type;

	unsigned char aux[MAX_SIZE];

	do {
		printf(".");
		Type= receiveframe(aux, &aux_num_chars);

		if(DEBUG) printf("aux = %s, aux_num_chars = %d\n",aux, aux_num_chars);

		if(Type == DATA_RECEIVED)
		{
			linkLayer.sequenceNumber = linkLayer.sequenceNumber ^ 1;
			sendSupervisionFrame(linkLayer.fd, RR);
			memcpy(buffer + num_chars_read, aux, aux_num_chars);
			num_chars_read += aux_num_chars;
			aux_num_chars = 0;
		}
		else if(Type == BAD_DATA_RECEIVED) {
			rej_send_received++;
			sendSupervisionFrame(linkLayer.fd, REJ);
		}
		else if(Type == DISC_RECEIVED) {
			printf("\n");
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

	linkLayer.alarm_char = DATA;

	for(i = 0; i < CompleteFrames; i++) write_frame(fd, buffer, 0, &i , FALSE);

	if(remainingBytes > 0) write_frame(fd, buffer, remainingBytes, &i, TRUE);

	(void) signal(SIGALRM, Timeout);
	printf("\n");
	llclose(linkLayer.fd);
	if(DEBUG) printf("[LLWRITE] END\n");
	return 0;

}

int llclose(int fd) {
	if(DEBUG) printf("[LLCLOSE] START (%d)\n", linkLayer.State);
	int tmp;
  (void) signal(SIGALRM, Timeout);
	linkLayer.alarm_char = DISC;

	if (linkLayer.State == TRANSMITTER) {
		sendSupervisionFrame(fd, DISC);
		alarm(linkLayer.timeout); //starting timout for DISC
		tmp = receiveframe(NULL,NULL); //waiting for DISC

		if (tmp == DISC_RECEIVED) {
			(void) signal(SIGALRM, SIG_IGN); //closing Timeout if received
			printf("[RS-232] Closing Port, successful transmission\n");
			sendSupervisionFrame(fd, UA);
			close(fd);
			return 1;

		}
		else{
			if(DEBUG) printf("[LLCLOSE] PROBLEMS (%x)\n", tmp);
			return -1;
		}
	}
	else if (linkLayer.State == RECEIVER) {

		linkLayer.alarm_char = DISC;
		sendSupervisionFrame(fd, DISC);
		alarm(linkLayer.timeout); //starting timout for DISC
		tmp = receiveframe(NULL,NULL);

		if (tmp == UA_RECEIVED) {
			(void) signal(SIGALRM, SIG_IGN); //closing Timeout if received
			printf("[RS-232] Closing Port, successful Recepcion\n");
			close(fd);
			return 1;
		}
		else{
			if(DEBUG) printf("[LLCLOSE] PROBLEMS (UA)\n");
			return -1;
		}
	}
	else {
	if(DEBUG) printf("[LLCLOSE] PROBLEMS\n");
	return -1;
	}
}
