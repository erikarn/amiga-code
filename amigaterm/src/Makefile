CC=m68k-amigaos-gcc
RM=rm
CFLAGS=-O -mcrt=nix13 -Wall -Werror
LDFLAGS=-mcrt=nix13

all: amigaterm

amigaterm.o: amigaterm.c

amigaterm_serial.o: amigaterm_serial.c

amigaterm_util.o: amigaterm_util.c

amigaterm: amigaterm.o amigaterm_serial.o amigaterm_util.o

clean:
	$(RM) -f amigaterm *.o
