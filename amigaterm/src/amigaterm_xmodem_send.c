/************************************************************************
 *  a terminal program that has ascii and xmodem transfer capability
 *
 *  use esc to abort xmodem transfer
 *
 *  written by Michael Mounier (1985)
 *  enhanced by Roc Valles Domenech (2018-2021)
 *  contributors: Alexander Fritsch (2021)
 ************************************************************************/
/*  compiler directives to fetch the necessary header files */
#include "dos/dos.h"              // for BPTR, MODE_NEWFILE, MODE_OLDFILE
#include "exec/io.h"              // for IOStdReq, CMD_READ, CMD_WRITE
#include "exec/memory.h"          // for MEMF_CLEAR, MEMF_PUBLIC
#include "exec/ports.h"           // for Message, MsgPort
#include "proto/dos.h"            // for Close, Open, Write, Read
#include "proto/exec.h"           // for FreeMem, DoIO, GetMsg, AllocMem
#include "stdlib.h"               // for exit
#include <clib/alib_protos.h>     // for DeletePort, BeginIO
#include <exec/types.h>           // for FALSE, TRUE, UBYTE, CONST_STRPTR
#include <stdio.h>                // for NULL, puts, fclose, fopen, EOF, getc
#include <stdbool.h>

#include "amigaterm_serial.h"
#include "amigaterm_serial_read.h"
#include "amigaterm_timer.h"
#include "amigaterm_util.h"
#include "amigaterm_xmodem.h"

#define BufSize 0x1000
static char bufr[BufSize];
#define ERRORMAX 10
#define RETRYMAX 10

/*
 * Anything using this will need to define an emits() function to print
 * a string.
 */
extern void emits(const char *);

int
XMODEM_Send_File(char *file)
{
  int sectnum, bytes_to_send, size, attempts;
  unsigned checksum, j, bufptr;
  unsigned char c;
  long bytes_xferred;
  serial_retval_t retval;
  BPTR fh;

  bytes_xferred = 0;
  if ((fh = Open((UBYTE *)file, MODE_OLDFILE)) < 0) {
    emits("Cannot Open Send File\n");
    return FALSE;
  } else
    emits("Sending File...");
  attempts = 0;
  sectnum = 1;
  /* wait for sync char */
  j = 1;
  do {
    c = 0;
    retval = readchar(&c);
    if (retval == SERIAL_RET_ABORT) {
      emits("\nUser cancelled transfer\n");
      goto error;
    }
  } while ((c != NAK) && (j++ < ERRORMAX));

#if 0
  emits("Got sync char\n");
#endif

  if (j >= (ERRORMAX)) {
    emits("\nReceiver not sending NAKs\n");
    goto error;
  }
  while ((bytes_to_send = Read(fh, bufr, BufSize)) && attempts != RETRYMAX) {
    if (bytes_to_send == EOF) {
      emits("\nError Reading File\n");
      goto error;
    };
    bufptr = 0;
    while (bytes_to_send > 0 && attempts != RETRYMAX) {
      attempts = 0;
      do {
        serial_write_char(SOH);
        serial_write_char(sectnum);
        serial_write_char(~sectnum);
        checksum = 0;
        size = SECSIZ <= bytes_to_send ? SECSIZ : bytes_to_send;
        bytes_to_send -= size;
        for (j = bufptr; j < (bufptr + SECSIZ); j++) {
          /*
           * Here's the bit that writes the file content out
           * as a bulk write.
           *
           * Note that it will fill the rest of the 128 byte
           * xmodem transfer with zeros if the send buffer
           * isn't big enough.
           *
           * This needs to be taken into account when attempting
           * to convert this particular spot to use a bulk async
           * write.
           */
          if (j < (bufptr + size)) {
            serial_write_char(bufr[j]);
            checksum += bufr[j];
          } else {
            serial_write_char(0);
          }
        }
        serial_write_char(checksum & 0xff);
        attempts++;
#if 0
        emits("Waiting for ACK/NACK\n");
#endif
        retval = readchar(&c);
        switch (retval) {
        case SERIAL_RET_OK:
          break;
        case SERIAL_RET_TIMEOUT:
          emits("\nTimeout waiting for ACK/NACK\n");
          c = 0;
          break;
        case SERIAL_RET_ABORT:
          goto error;
        }
      } while ((c != ACK) && (attempts != RETRYMAX));
      bufptr += size;
      bytes_xferred += size;
#if 0
      emits("Block ");
      stci_d(numb,sectnum,i);
      snprintf(numb, 10, "%u", sectnum);
      emits(numb);
      emits(" sent\n");
#endif
      sectnum++;
    }
  }
  Close(fh);
  if (attempts == RETRYMAX) {
    emits("\nNo Acknowledgment Of Sector, Aborting\n");
    goto error;
  } else {
    attempts = 0;
    bool timeout = false;
    do {
      serial_write_char(EOT);
      attempts++;
      retval = readchar(&c);
      switch (retval) {
      case SERIAL_RET_OK:
        break;
      case SERIAL_RET_ABORT:
        goto error;
      case SERIAL_RET_TIMEOUT:
        timeout = true;
        break;
      }
    } while ((c != ACK) && (attempts != RETRYMAX) &&
      (timeout == false));
    if (attempts == RETRYMAX)
      emits("\nNo Acknowledgment Of End Of File\n");
  };
  return TRUE;
error:
  Close(fh);
  return FALSE;
}
