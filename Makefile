CC = gcc
CFLAGS = -Ofast -march=native -Wall -Wextra
LDFLAGS = -flto

.PHONY: all

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: bench

bench: klibc.o illumos.o uclibc.o plan9.o mine.o musl.o diet.o bsd.o qsort.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) qsort.c *.o -o qsort
