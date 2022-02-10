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
#include "proto/exec.h"           // for FreeMem, DoIO, GetMsg, AllocMem
#include "clib/alib_protos.h"     // for DeletePort, BeginIO
#include "devices/serial.h"       // for IOExtSer, SERF_SHARED, SERF_XDISA...
#include "exec/types.h"           // for FALSE, TRUE, UBYTE, CONST_STRPTR
#include <stdio.h>                // for NULL, puts, fclose, fopen, EOF, getc

#include "amigaterm_serial.h"

/* Whether to use hardware flow control or not */
#define ENABLE_HWFLOW  1

/* declarations for the serial stuff */

struct IOExtSer *Read_Request;
char rs_in[2];
struct IOExtSer *Write_Request;
char rs_out[2];

int
serial_init(void)
{
  Read_Request = (struct IOExtSer *)AllocMem(sizeof(*Read_Request),
                                             MEMF_PUBLIC | MEMF_CLEAR);
  Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
#if ENABLE_HWFLOW
  Read_Request->io_SerFlags |= SERF_7WIRE;
#endif
  Read_Request->IOSer.io_Flags = 0;
  Read_Request->IOSer.io_Message.mn_ReplyPort =
      CreatePort((CONST_STRPTR) "Read_RS", 0);
  if (OpenDevice((CONST_STRPTR)SERIALNAME, 0, (struct IORequest *)Read_Request,
                 0)) {
    puts("Can't open Read device\n");
    DeletePort(Read_Request->IOSer.io_Message.mn_ReplyPort);
    FreeMem(Read_Request, sizeof(*Read_Request));

    return 0;
  }

  Read_Request->IOSer.io_Command = CMD_READ;
  Read_Request->IOSer.io_Length = 1;
  Read_Request->IOSer.io_Data = (APTR)&rs_in[0];
  Read_Request->IOSer.io_Flags = 0;

  Write_Request = (struct IOExtSer *)AllocMem(sizeof(*Write_Request),
                                              MEMF_PUBLIC | MEMF_CLEAR);
  Write_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
#if ENABLE_HWFLOW
  Write_Request->io_SerFlags |= SERF_7WIRE;
#endif
  Write_Request->IOSer.io_Message.mn_ReplyPort =
      CreatePort((CONST_STRPTR) "Write_RS", 0);
  if (OpenDevice((CONST_STRPTR)SERIALNAME, 0, (struct IORequest *)Write_Request,
                 0)) {
    puts("Can't open Write device\n");
    DeletePort(Write_Request->IOSer.io_Message.mn_ReplyPort);
    FreeMem(Write_Request, sizeof(*Write_Request));
    DeletePort(Read_Request->IOSer.io_Message.mn_ReplyPort);
    FreeMem(Read_Request, sizeof(*Read_Request));

    return 0;
  }
  Write_Request->IOSer.io_Command = CMD_WRITE;
  Write_Request->IOSer.io_Length = 1;
  Write_Request->IOSer.io_Data = (APTR)&rs_out[0];
  Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
#if ENABLE_HWFLOW
  Read_Request->io_SerFlags |= SERF_7WIRE;
#endif
  Read_Request->io_Baud = 9600;
  Read_Request->io_ReadLen = 8;
  Read_Request->io_WriteLen = 8;
  Read_Request->io_CtlChar = 1L;
  Read_Request->IOSer.io_Flags = 0;
  Read_Request->IOSer.io_Command = SDCMD_SETPARAMS;
  DoIO((struct IORequest *)Read_Request);
  Read_Request->IOSer.io_Command = CMD_READ;

  return (1);
}

/*
 * Queue an IO to read a single character.
 *
 * This kick starts the async read, but it doesn't wait; we'll get
 * a signal when the IO is ready.
 *
 * This will use IOF_QUICK, so you should use the wrapper functions
 * here to read the serial data; it'll do the right thing.
 */
void
serial_read_start(void)
{
  Read_Request->IOSer.io_Command = CMD_READ;
  Read_Request->IOSer.io_Length = 1;
  Read_Request->IOSer.io_Data = (APTR)&rs_in[0];
  Read_Request->IOSer.io_Flags = IOF_QUICK;
  BeginIO((struct IORequest *)Read_Request);
}

/*
 * Get the signal bitmask to wait on for serial IO.
 */
unsigned int
serial_get_signal_bitmask(void)
{
    return (1 << Read_Request->IOSer.io_Message.mn_ReplyPort->mp_SigBit);
}

/*
 * Write a single character to the serial port.
 *
 * This blocks; we currently don't support non-blocking serial
 * writes here.
 */
void
serial_write_char(char c)
{
    rs_out[0] = c;
    DoIO((struct IORequest *)Write_Request);
}

/*
 * Abort any pending read IO.
 */
void
serial_read_abort(void)
{
    AbortIO((struct IORequest *)Read_Request);
}

void
serial_set_baud(int baud)
{
    Read_Request->io_Baud = baud;
    Read_Request->IOSer.io_Command = SDCMD_SETPARAMS;
    Read_Request->IOSer.io_Flags = 0;
    DoIO((struct IORequest *)Read_Request);
    Read_Request->IOSer.io_Command = CMD_READ;
}

void
serial_close(void)
{
  /*
   * XXX Do we need to abort a pending read IO?
   * The original code did not; maybe that's a bug?
   * (but it does mean we should track if we've issued
   * a read or not..)
   */

  CloseDevice((struct IORequest *)Read_Request);
  DeletePort(Read_Request->IOSer.io_Message.mn_ReplyPort);
  FreeMem(Read_Request, sizeof(*Read_Request));

  CloseDevice((struct IORequest *)Write_Request);
  DeletePort(Write_Request->IOSer.io_Message.mn_ReplyPort);
  FreeMem(Write_Request, sizeof(*Write_Request));
}

/*
 * Return 1 if the IO is ready.
 */
static int
serial_read_ready(void)
{
    if (Read_Request->IOSer.io_Flags & IOF_QUICK)
      return 1;

    if (CheckIO((struct IORequest *) Read_Request))
        return 1;
    return 0;
}

static int
serial_read_wait(void)
{
    char ret;

    if (Read_Request->IOSer.io_Flags & IOF_QUICK)
      return 1;

    ret = WaitIO((struct IORequest *) Read_Request);
    if (ret == 0)
        return 1;
    return 0;
}

/*
 * Check to see if the pending serial IO is ready and if
 * so, read it and return it.
 *
 * This returns -1 if there was no serial character ready,
 * and the character if there was.
 *
 * This routine won't queue the IO for the next character.
 * The caller should use serial_read_start() again to do that.
 */
int
serial_get_char(void)
{
    if (serial_read_ready() == 0)
        return -1;
    if (serial_read_wait() == 0)
        return -1;
    return ((int)rs_in[0]) & 0xff;
}

/*
 * Call to see if the read is already ready.  This is called
 * as IOF_QUICK transactions won't post a signal.
 */
int
serial_read_is_ready(void)
{
    if (Read_Request->IOSer.io_Flags & IOF_QUICK)
      return 1;
    return 0;
}
