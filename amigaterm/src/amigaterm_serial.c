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

static struct IOExtSer *Read_Request;
static char rs_in[2];
static struct IOExtSer *Write_Request;
static char rs_out[2];

static char read_queued = 0;

int
serial_init(int baud)
{

  // XXX TODO: use CreateExtIO / DeleteExtIO!
  Read_Request = (struct IOExtSer *)AllocMem(sizeof(*Read_Request),
                                             MEMF_PUBLIC | MEMF_CLEAR);
  Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
#if ENABLE_HWFLOW
  Read_Request->io_SerFlags |= SERF_7WIRE;
#endif
  Read_Request->IOSer.io_Flags = 0;
  // XXX TODO: once creatextio is used, we don't need this; it's done in createextio
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
  Read_Request->io_Baud = baud;
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
serial_read_start(char *l)
{
  if (read_queued == 1) {
      printf("%s: called (%s), read_queued is 1!\n", __func__, l);
  }
  Read_Request->IOSer.io_Command = CMD_READ;
  Read_Request->IOSer.io_Length = 1;
  Read_Request->IOSer.io_Data = (APTR)&rs_in[0];
  Read_Request->IOSer.io_Flags = IOF_QUICK;
  BeginIO((struct IORequest *)Read_Request);
  read_queued = 1;
}

/*
 * Queue an IO to read into the given buf.
 *
 * This will kick-start an async read and not wait, but for
 * a larger buffer.
 *
 * This will use IOF_QUICK, so you should use the wrapper functions
 * here to check for the serial data and then complete the IO
 * so IOF_QUICK is correctly handled.
 */
void
serial_read_start_buf(char *buf, int len)
{

  if (read_queued == 1) {
      printf("%s: called, read_queued is 1!\n", __func__);
  }
  Read_Request->IOSer.io_Command = CMD_READ;
  Read_Request->IOSer.io_Length = len;
  Read_Request->IOSer.io_Data = (APTR) buf;
  Read_Request->IOSer.io_Flags = IOF_QUICK;
  BeginIO((struct IORequest *)Read_Request);
  read_queued = 1;
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
    if (read_queued == 1)
        AbortIO((struct IORequest *)Read_Request);
    read_queued = 0;
}

void
serial_set_baud(int baud)
{
    if (read_queued == 1) {
      printf("%s: called with read_queued=1!\n", __func__);
    }
    Read_Request->io_Baud = baud;
    Read_Request->IOSer.io_Command = SDCMD_SETPARAMS;
    Read_Request->IOSer.io_Flags = 0;
    DoIO((struct IORequest *)Read_Request);
    Read_Request->IOSer.io_Command = CMD_READ;
}

void
serial_close(void)
{
  serial_read_abort();

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
int
serial_read_ready(void)
{
    if (Read_Request->IOSer.io_Flags & IOF_QUICK)
      return 1;

    if (CheckIO((struct IORequest *) Read_Request))
        return 1;
    return 0;
}

/*
 * Wait for the IO to complete.
 *
 * Returns 1 if the IO is ready, 0 if there was an error.
 */
int
serial_read_wait(void)
{
    char ret;

    if (Read_Request->IOSer.io_Flags & IOF_QUICK) {
      read_queued = 0;
      return 1;
    }

    ret = WaitIO((struct IORequest *) Read_Request);
    if (ret == 0) {
        read_queued = 0;
        return 1;
    }

    /*
     * We definitely, DEFINITELY have to handle the error
     * here.  Eg I'm seeing '6' returned here, which is a
     * hardware overrun.  At this point I don't know whether
     * we need to abort the existing IO or whether it's
     * considered as 'failed'.
     */
    printf("%s: WaitIO failed (%d)\n", __func__, ret);
    read_queued = 0;
    return 0;
}

/*
 * Check to see if the pending serial IO is ready and if so
 * fetch it and return it.
 *
 * This routine won't queue the IO for the next character.
 * The caller should use serial_read_start() again to do that.
 *
 * Returns 0 if no character is ready, 1 if character is ready
 * and -1 if there was a read error.
 */
int
serial_get_char(char *ch)
{

    if (read_queued == 0) {
        printf("%s: called; read_queued=0?\n", __func__);
    }

    if (serial_read_ready() == 0)
        return 0;

    /*
     * XXX yes we need to return an actual error here, as
     * if serial_read_wait() returned an error (eg overrun)
     * the transaction is likely "complete" and we'll have
     * to return an error up to the caller to handle AND
     * have them re-schedule an IO.
     */
    if (serial_read_wait() == 0)
        return -1;
    read_queued = 0;

    *ch = rs_in[0];
    return 1;
}

/*
 * Call to see if the read is already ready.  This is called
 * as IOF_QUICK transactions won't post a signal.
 */
int
serial_read_is_ready(void)
{
    if (Read_Request->IOSer.io_Flags & IOF_QUICK) {
      return 1;
    }
    return 0;
}
