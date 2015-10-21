all:
	gcc -c linklayer.c linklayer.h main.c
	gcc linklayer.o main.o -o src
