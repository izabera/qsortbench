CC = gcc
CFLAGS = -Ofast -march=native -Wall -Wextra -std=c11 # -Dcheck=1
#CFLAGS = -Os -march=native -Wall -Wextra -std=c11 # -Dcheck=1
LDFLAGS = -flto

.PHONY: all clean

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: bench

clean:
	rm -rf *.o qsort

bench: reactos.o ms.o linux.o sortix.o freebsd.o glibc.o wada.o klibc.o illumos.o uclibc.o plan9.o mine.o mini.o musl.o diet.o bsd.o qsort.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) qsort.c *.o -o qsort
