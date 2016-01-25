# i'm not very good at makefiles
CC=gcc
CFLAGS=-c -Wall

all: test

test.o : test.c
	$(CC) $(CFLAGS) test.c
	
fat.o : fat.c
	$(CC) $(CFLAGS) fat.c
	
test: test.o fat.o
	$(CC) test.o fat.o -o test
	
clean:
	rm -f *.o

