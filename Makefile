CFLAGS = -pg -Wall -O3 -lm
CC = c11

.PHONY: clean

all: main.c huffman.c
	gcc -o main main.c huffman.c -I. $(CFLAGS) -std=$(CC) 
