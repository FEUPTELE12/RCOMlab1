all:
	gcc -c linklayer.c linklayer.h application.c
	gcc linklayer.o application.o -o src
