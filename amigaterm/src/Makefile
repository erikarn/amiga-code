CC=m68k-amigaos-gcc
RM=rm
CFLAGS=-O -mcrt=nix13 -Wall -Werror
LDFLAGS=-mcrt=nix13 -L../lib/timer

all: amigaterm

amigaterm.o: amigaterm.c

amigaterm_serial.o: amigaterm_serial.c

amigaterm_screen.o: amigaterm_screen.c

amigaterm_serial_read.o: amigaterm_serial_read.c

amigaterm_util.o: amigaterm_util.c

amigaterm_xmodem_recv.o: amigaterm_xmodem_recv.c

amigaterm_xmodem_send.o: amigaterm_xmodem_send.c

amigaterm: amigaterm.o amigaterm_serial.o amigaterm_util.o \
	   amigaterm_serial_read.o \
	   amigaterm_xmodem_recv.o amigaterm_xmodem_send.o \
	   amigaterm_screen.o ../lib/timer/libtimer.a
clean:
	$(RM) -f amigaterm *.o
