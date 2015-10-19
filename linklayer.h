/*llopen*/



int llopen(char* port, int txrx);

int llread(int fd, char* buffer);

int llwrite(int fd, unsigned char* buffer, int length);

int llclose(int fd);

