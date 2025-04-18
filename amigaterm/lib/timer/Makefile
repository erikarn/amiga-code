CC=m68k-amigaos-gcc
AR=m68k-amigaos-ar
RANLIB=m68k-amigaos-ranlib
RM=rm
CFLAGS=-O -mcrt=nix13 -Wall -Werror
LDFLAGS=-mcrt=nix13

all: libtimer.a

libtimer.a: timer.o
	$(AR) ru $@ $^
	$(RANLIB) $@

clean:
	$(RM) -f *.a *.o
