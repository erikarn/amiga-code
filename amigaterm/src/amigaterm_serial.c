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

/* declarations for the serial stuff */

static struct IOExtSer *Read_Request = NULL;
static struct IOExtSer *Write_Request = NULL;
static struct MsgPort *serial_read_port = NULL;
static struct MsgPort *serial_write_port = NULL;

static char rs_in[2];
static char rs_out[2];

static char read_queued = 0;
static char write_queued = 0;

int
serial_init(int baud, int enable_hwflow)
{

  /* Create two ports - one for serial read, one for serial write */
  serial_read_port = CreatePort((CONST_STRPTR) "Read_RS", 0);
  if (serial_read_port == NULL) {
    goto error;
  }

  serial_write_port = CreatePort((CONST_STRPTR) "Write_RS", 0);
  if (serial_write_port == NULL) {
    goto error;
  }

  /* Allocate read request */
  Read_Request = (struct IOExtSer *) CreateExtIO(serial_read_port,
    sizeof(struct IOExtSer));
  if (Read_Request == NULL) {
    goto error;
  }

  Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
  if (enable_hwflow) {
    Read_Request->io_SerFlags |= SERF_7WIRE;
  }

  Read_Request->IOSer.io_Flags = 0;

  if (OpenDevice((CONST_STRPTR)SERIALNAME, 0, (struct IORequest *)Read_Request,
                 0)) {
    puts("Can't open Read device\n");
    goto error;
  }

  Read_Request->IOSer.io_Command = CMD_READ;
  Read_Request->IOSer.io_Length = 1;
  Read_Request->IOSer.io_Data = (APTR)&rs_in[0];
  Read_Request->IOSer.io_Flags = 0;

  /* Allocate write request */
  Write_Request = (struct IOExtSer *) CreateExtIO(serial_write_port,
    sizeof(struct IOExtSer));
  if (Write_Request == NULL) {
    goto error;
  }

  Write_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
  if (enable_hwflow) {
    Write_Request->io_SerFlags |= SERF_7WIRE;
  }

  if (OpenDevice((CONST_STRPTR)SERIALNAME, 0, (struct IORequest *)Write_Request,
                 0)) {
    puts("Can't open Write device\n");
    CloseDevice((struct IORequest *)Read_Request);
    goto error;
  }

  Write_Request->IOSer.io_Command = CMD_WRITE;
  Write_Request->IOSer.io_Length = 1;
  Write_Request->IOSer.io_Data = (APTR)&rs_out[0];

  Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
  Read_Request->io_SerFlags |= SERF_7WIRE;
  if (enable_hwflow) {
    Read_Request->io_SerFlags |= SERF_7WIRE;
  }

  Read_Request->io_Baud = baud;
  Read_Request->io_ReadLen = 8;
  Read_Request->io_WriteLen = 8;
  Read_Request->io_CtlChar = 1L;
  Read_Request->IOSer.io_Flags = 0;
  Read_Request->IOSer.io_Command = SDCMD_SETPARAMS;
  DoIO((struct IORequest *)Read_Request);

  Read_Request->IOSer.io_Command = CMD_READ;

  return (1);

error:
  if (Read_Request != NULL) {
    DeleteExtIO((struct IORequest *) Read_Request);
  }

  if (serial_read_port != NULL) {
    DeletePort(serial_read_port);
  }

  if (Write_Request != NULL) {
    DeleteExtIO((struct IORequest *) Write_Request);
  }

  if (serial_write_port != NULL) {
    DeletePort(serial_write_port);
  }

  return (0);
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
  if (read_queued == 1)
      puts("serial_read_start: called w/ read_queued=1!\n");

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

  if (read_queued == 1)
      puts("serial_read_start_buf: called w/ read_queued=1!\n");

  Read_Request->IOSer.io_Command = CMD_READ;
  Read_Request->IOSer.io_Length = len;
  Read_Request->IOSer.io_Data = (APTR) buf;
  Read_Request->IOSer.io_Flags = IOF_QUICK;
  BeginIO((struct IORequest *)Read_Request);
  read_queued = 1;
}

/*
 * Get the signal bitmask to wait on for read serial IO.
 */
unsigned int
serial_get_read_signal_bitmask(void)
{
    return (1 << Read_Request->IOSer.io_Message.mn_ReplyPort->mp_SigBit);
}

/*
 * Abort any pending read IO.
 */
void
serial_read_abort(void)
{
    if (read_queued == 1) {
        AbortIO((struct IORequest *)Read_Request);
        WaitIO((struct IORequest *) Read_Request);
        SetSignal(0, serial_get_read_signal_bitmask());
    }
    read_queued = 0;
}

void
serial_set_baud(int baud)
{
    if (read_queued == 1)
      puts("serial_set_baud: called w/ read_queued=1!\n");

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
  serial_write_abort();

  CloseDevice((struct IORequest *)Read_Request);
  CloseDevice((struct IORequest *)Write_Request);

  DeleteExtIO((struct IORequest *) Read_Request);
  DeletePort(serial_read_port);

  DeleteExtIO((struct IORequest *) Write_Request);
  DeletePort(serial_write_port);
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

    /* Will WaitIO do the right thing with IOF_QUICK for us? */
#if 0
    if (Read_Request->IOSer.io_Flags & IOF_QUICK) {
      read_queued = 0;
      return 1;
    }
#endif
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
//    printf("%s: WaitIO failed (%d)\n", __func__, ret);
    AbortIO((struct IORequest *) Read_Request); // ?
    SetSignal(0, serial_get_read_signal_bitmask());
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
serial_get_char(unsigned char *ch)
{

    if (read_queued == 0)
        puts("serial_get_char: called w/ read_queued=0!\n");

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

    *ch = (unsigned char) rs_in[0];
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


/* *************************************** */

/*
 * Serial write IO routines
 */

/*
 * Abort any pending write IO.
 */
void
serial_write_abort(void)
{
    if (write_queued == 1) {
        AbortIO((struct IORequest *)Write_Request);
        WaitIO((struct IORequest *) Write_Request);
        SetSignal(0, serial_get_write_signal_bitmask());
    }
    write_queued = 0;
}

/*
 * Get the signal bitmask to wait on for write serial IO.
 */
unsigned int
serial_get_write_signal_bitmask(void)
{
    return (1 << Write_Request->IOSer.io_Message.mn_ReplyPort->mp_SigBit);
}

/*
 * Write a single character to the serial port.
 *
 * This blocks until completion or error.
 */
void
serial_write_char(char c)
{
    rs_out[0] = c;
    Write_Request->IOSer.io_Command = CMD_WRITE;
    Write_Request->IOSer.io_Data = (APTR)&rs_out[0];
    Write_Request->IOSer.io_Length = 1;
    DoIO((struct IORequest *)Write_Request);
}

/*
 * Call to see if the write is already completed.
 * This is called as IOF_QUICK transactions won't post a signal.
 *
 * Only call this if an IO has been scheduled - ie if you
 * know that you're doing a non-blocking IO.
 */
int
serial_write_is_ready(void)
{
    if (Write_Request->IOSer.io_Flags & IOF_QUICK) {
      return 1;
    }
    return 0;
}

/*
 * Return 1 if the write IO is ready.
 *
 * Only call this if an IO has been scheduled - ie if you
 * know that you're doing a non-blocking IO.
 */
int
serial_write_ready(void)
{
    if (Write_Request->IOSer.io_Flags & IOF_QUICK)
      return 1;

    if (CheckIO((struct IORequest *) Write_Request))
        return 1;
    return 0;
}

/*
 * Wait for the write IO to complete.
 *
 * Returns 1 if the IO is ready, 0 if there was an error.
 */
int
serial_write_wait(void)
{
    char ret;

    if (write_queued == 0) {
        puts("serial_write_wait: called w/out write queued\n");
    }

    ret = WaitIO((struct IORequest *) Write_Request);
    if (ret == 0) {
        write_queued = 0;
        return 1;
    }

    /* Handle an IO error - eg like a hardware error */
    printf("%s: WaitIO failed (%d)\n", __func__, ret);
    AbortIO((struct IORequest *) Write_Request); // ?
    SetSignal(0, serial_get_write_signal_bitmask());
    write_queued = 0;
    return 0;
}

/*
 * Queue an IO to write into the given buf.
 *
 * This will kick-start an async write and not wait, but for
 * a larger buffer.
 *
 * This will use IOF_QUICK, so you should use the wrapper functions
 * here to check for the serial data and then complete the IO
 * so IOF_QUICK is correctly handled.
 *
 * It's important that the sync version of this routine isn't
 * called whilst this async version is in progress.
 */
void
serial_write_start_buf(char *buf, int len)
{

  if (write_queued == 1)
      puts("serial_write_start_buf: called w/ write_queued=1!\n");

  Write_Request->IOSer.io_Command = CMD_WRITE;
  Write_Request->IOSer.io_Length = len;
  Write_Request->IOSer.io_Data = (APTR) buf;
  Write_Request->IOSer.io_Flags = IOF_QUICK;
  BeginIO((struct IORequest *) Write_Request);
  write_queued = 1;
}

