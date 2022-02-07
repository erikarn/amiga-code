CC=m68k-amigaos-gcc
RM=rm
CFLAGS=-O -mcrt=nix13 -Wall
LDFLAGS=-mcrt=nix13

all: fixobj

fixobj.o: fixobj.c

fixobj: fixobj.o

clean:
	$(RM) -f fixobj fixobj.o
