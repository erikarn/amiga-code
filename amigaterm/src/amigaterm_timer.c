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
#include "devices/timer.h"
#include "exec/types.h"           // for FALSE, TRUE, UBYTE, CONST_STRPTR
#include <stdio.h>                // for NULL, puts, fclose, fopen, EOF, getc

#include "amigaterm_timer.h"

struct Device * TimerBase;
static struct timerequest *timer_req;
static int pending_timer = 0;
static struct MsgPort *timer_port;

int
timer_init(void)
{
    int ret;

    timer_port = CreatePort(NULL, 0);
    if (timer_port == NULL)
        return (0);

    timer_req = (struct timerequest *) CreateExtIO(timer_port,
      sizeof(struct timerequest));
    if (timer_req == NULL) {
        DeletePort(timer_port);
        return (0);
    }

    ret = OpenDevice((CONST_STRPTR) TIMERNAME, 0, (struct IORequest *) timer_req, 0);

    if (ret != 0) {
        DeleteExtIO((struct IORequest *) timer_req);
        DeletePort(timer_port);
        puts("Couldn't open timer device\n");
        return 0;
    }

    TimerBase = timer_req->tr_node.io_Device;

    return 1;
}

void
timer_close(void)
{
    if (pending_timer) {
        AbortIO((struct IORequest *) timer_req);
        WaitIO((struct IORequest *) timer_req);
        SetSignal(0, timer_get_signal_bitmask());
    }

    CloseDevice((struct IORequest *) timer_req);
    DeleteExtIO((struct IORequest *) timer_req);
    DeletePort(timer_port);
}

void
timer_timeout_set(int ms)
{
    if (pending_timer) {
        AbortIO((struct IORequest *) timer_req);
        WaitIO((struct IORequest *) timer_req);
        SetSignal(0, timer_get_signal_bitmask());
    }

    timer_req->tr_time.tv_sec = ms / 1000;
    timer_req->tr_time.tv_usec = ms % 1000;
    timer_req->tr_node.io_Command = TR_ADDREQUEST;

    SendIO((struct IORequest *) timer_req);
    pending_timer = 1;
}

unsigned int
timer_get_signal_bitmask(void)
{

    return (1 << timer_port->mp_SigBit);
}

void
timer_timeout_abort(void)
{
    if (pending_timer) {
        AbortIO((struct IORequest *) timer_req);
        WaitIO((struct IORequest *) timer_req);
        SetSignal(0, timer_get_signal_bitmask());
    }
    pending_timer = 0;
}

int
timer_timeout_fired(void)
{
    if (pending_timer == 0)
        return 0;
    return (CheckIO((struct IORequest *) timer_req) != 0);
}

int
timer_timeout_complete(void)
{
    if (timer_timeout_fired() != 1)
        return 0;

    WaitIO((struct IORequest *) timer_req);
    pending_timer = 0;
    return 1;
}
