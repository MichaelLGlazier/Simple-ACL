CC=gcc
CFLAGS= -g -pedantic-errors -Wall -std=c99

default: all	
	$(CC) -o fritter fritter.o

all:		fritter.o fritter.o

netrecon.o:	fritter.c
	$(CC) $(CFLAGS) -c fritter.c

.PHONY: clean

clean:
	rm -rf fritter fritter.o