CC=m68k-amigaos-gcc
RM=rm
CFLAGS=-O -mcrt=nix13 -Wall
LDFLAGS=-mcrt=nix13

all: amigaterm

amigaterm.o: amigaterm.c

amigaterm_serial.o: amigaterm_serial.c

amigaterm: amigaterm.o amigaterm_serial.o

clean:
	$(RM) -f amigaterm amigaterm.o amigaterm_serial.o
