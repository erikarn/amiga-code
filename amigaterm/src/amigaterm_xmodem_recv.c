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

#include "amigaterm_serial.h"
#include "amigaterm_serial_read.h"
#include "amigaterm_timer.h"
#include "amigaterm_util.h"
#include "amigaterm_xmodem.h"

#define BufSize 0x1000
static char bufr[BufSize];
#define ERRORMAX 10

/*
 * Anything using this will need to define an emits() function to print
 * a string.
 */
extern void emits(const char *);

/*
 * Figure out how many bytes we need to write for this particular
 * transfer.  If file_size is 0 or -1 then it's always the block
 * size, else we ensure only enough bytes are written to satisfy
 * file_size.
 */
static int
get_bytes_for_transfer(long file_size, long file_offset, int block_size)
{
  long bw;

  if (file_size < 1)
    return block_size;

  bw = file_size - file_offset;
  if (bw < block_size)
    return bw;
  return block_size;
}

/***************************************/
/*  xmodem send and receive functions */
/*************************************/

/*
 * Xmodem receive.
 *
 * This doesn't at /all/ handle missing characters from the stream,
 * any form of timeout/resyncing the streams, etc.
 */
int XMODEM_Read_File(char *file, long file_size) {
  long file_offset = 0L;
  long bytes_xferred;
  int sectnum, errors, errorflag;
  unsigned int j, bufptr;
  int bw;
  serial_retval_t retval;
  unsigned char firstchar, sectcurr, sectcomp, checksum, checkcmp;
  BPTR fh;

  bytes_xferred = 0L;
  if ((fh = Open((UBYTE *)file, MODE_NEWFILE)) < 0) {
    emits("Cannot Open File\n");
    return FALSE;
  } else {
    emits("Receiving File...\n");
  }

  sectnum = errors = bufptr = 0;

  // Flush everything first before we kick the remote side
  readchar_flush(100);

  /* Kick the remote side to start sending */
  serial_write_char(NAK);
  firstchar = 0;

  /* Loop until we're done or hit maximum errors */
  while (firstchar != EOT && errors != ERRORMAX) {
    errorflag = FALSE;

    /* Loop over until we hit sync char or EOT */
    do {
      retval = readchar(&firstchar);

      /* Only bail out here if we abort, timeout is OK */
      if (retval == SERIAL_RET_ABORT) {
        goto error;
      }
    } while (firstchar != SOH && firstchar != EOT);

    /* If we're at SOH then start reading the current block */
    if (firstchar == SOH) {
      /* Read the current sector and its inverted value */
      retval = readchar(&sectcurr);
      switch (retval) {
      case SERIAL_RET_OK:
        break;
      case SERIAL_RET_ABORT:
        goto error;
      case SERIAL_RET_TIMEOUT:
        readchar_flush(100);
        continue;
      }

      retval = readchar(&sectcomp);
      switch (retval) {
      case SERIAL_RET_OK:
        break;
      case SERIAL_RET_ABORT:
        goto error;
      case SERIAL_RET_TIMEOUT:
        readchar_flush(100);
        continue;
      }

      if ((sectcurr + sectcomp) == 255) {
        /* Check to see if this sector is the next we're expecting */
        if (sectcurr == ((sectnum + 1) & 0xff)) {
          checksum = 0;
          /* Read the 128 byte data block */
          retval = readchar_buf(&bufr[bufptr], SECSIZ);
          switch (retval) {
          case SERIAL_RET_OK:
            break;
          case SERIAL_RET_ABORT:
            goto error;
          case SERIAL_RET_TIMEOUT:
            emits("Timeout receiving block\n");
            readchar_flush(100);
            serial_write_char(NAK);
            errors++;
            continue;
          }

          /* Calculate the checksum */
          for (j = bufptr; j < (bufptr + SECSIZ); j++) {
              checksum = (checksum + bufr[j]) & 0xff;
          }

          /* Read / check checksum */
          retval = readchar(&checkcmp);
          switch (retval) {
          case SERIAL_RET_OK:
            break;
          case SERIAL_RET_ABORT:
            goto error;
          case SERIAL_RET_TIMEOUT:
            emits("Timeout receiving checksum\n");
            readchar_flush(100);
            serial_write_char(NAK);
            errors++;
            continue;
          }

          if (checksum == checkcmp) {
            errors = 0;
            sectnum++;
            bufptr += SECSIZ;
            bytes_xferred += SECSIZ;
            /* Verified! */
            if (bufptr == BufSize) {
              bufptr = 0;
              bw = get_bytes_for_transfer(file_size, file_offset, BufSize);
              if ((bw > 0) && (Write(fh, bufr, bw) == EOF)) {
                emits("Error Writing File\n");
                goto error;
              };
              file_offset += bw;
            };
            serial_write_char(ACK);
          } else {
            emits("Invalid checksum\n");
            errorflag = TRUE;
          }
        } else {
          if (sectcurr == (sectnum & 0xff)) {
            emits("Received Duplicate Sector\n");
            serial_write_char(ACK);
          } else {
            emits("Wrong sector offset\n");
            errorflag = TRUE;
          }
        }
      } else {
        emits("Invalid sector bytes\n");
        errorflag = TRUE;
      }
    }

    if (errorflag == TRUE) {
      errors++;
      emits("Sending NAK\n");
      readchar_flush(100);
      serial_write_char(NAK);
    }
  }; /* end while */

  if ((firstchar == EOT) && (errors < ERRORMAX)) {
    serial_write_char(ACK);
    bw = get_bytes_for_transfer(file_size, file_offset, bufptr);
    if (bw > 0)
        Write(fh, bufr, bw);
    Close(fh);
    emits("\nReceive OK\n");
    return TRUE;
  }
  emits("\nReceive fail\n");

error:
  /*
   * Do a flush here to eat any half-read buffer
   * before we return.
   */
  readchar_flush(500);
  Close(fh);
  return FALSE;
}
