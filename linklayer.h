/*RS-232 linklayer header*/

//Constantes can be modified in application layer
extern long BAUDRATE;
extern long MAXR ;    //Maximum number atents
extern long MAXT;     // Timeout value in seconds
extern long MAX_SIZE; //Maximum data to send in a frame
extern long transmited;
extern long received;
extern long timeout_time;
extern long rej_send_received;
/*****************************************************
 *                                                   *
 *     FUNCIONS TO BE USED IN APPLICATION LAYER      *
 *                                                   *
 *****************************************************/

/*
 * Open the connection
 * Arguments:
 * 	        -> port: Port to open
 * 	        -> txrx: Type of sender (1 = TRANSMITOR / 0 = RECIVER)
 * Return :
 * 	        -> -1 if error
 *          -> Identificator of data connection
 */

int llopen(char* port, int txrx);

/*
 * Read buffer from data connection
 * Arguments:
 * 	        -> fd: Data connection Identificator
 * 	        -> buffer: Buffer for saving informacion
 * Return :
 * 	        -> -1 if error
 *          -> bytes read
 */

int llread(int fd, unsigned char* buffer);

/*
 * Writing to datalink connection
 * Arguments:
 * 	        - fd: Data connection Identificator
 * 	        - buffer:  array chars to transmitt
 *          - length:  array size
 * Return :
 * 	        - -1 if error
 *          - bytes written
 */
int llwrite(int fd, unsigned char* buffer, int length);

/*
 * Writing to datalink connection
 * Arguments:
 * 	        -> fd: Data connection Identificator
 * Return :
 * 	        -> -1 if error
 *          -> 1 if success
 */
int llclose(int fd);

/** end header **/
