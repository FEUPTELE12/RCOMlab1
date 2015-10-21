/*llopen*/

extern long BAUDRATE;
extern long MAXR 3 //no maximo de falhas
extern long MAXT 1 //valor do temporizador
extern long MAX_SIZE 256 //Tamanho maximo de uma frame APÓS stuffing

/*
 * Open the connection
 * Receives:
 * 	-> Port 
 * 	-> Type of sender (TRANSMITOR / RECIVER)
 * Returns :
 * 	-> -1 if error
 *  -> Identificator of data connection
 */
 
int llopen(char* port, int txrx);

/*
 * Open the connection
 * 
 * 
 */
 
int llread(int fd, unsigned char* buffer);
/*
 * Open the connection
 * 
 * 
 */
int llwrite(int fd, unsigned char* buffer, int length);

/*
 * Open the connection
 * 
 * 
 */
int llclose(int fd);

