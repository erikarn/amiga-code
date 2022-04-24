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
#include "amigaterm_timer.h"
#include "amigaterm_util.h"

#include "amigaterm_serial_read.h"

/*
 * Things we link to need to define these.
 */
extern bool serial_read_check_keypress_fn(void);
extern unsigned int serial_get_abort_keypress_signal_bitmask(void);
extern void emits(const char *);

/*
 * Empty the receive buffer until it times out.
 *
 * This is called in the error path if we get a receive
 * error (eg a hardware error) during packet receive.
 * This will keep looping over and reading data in the most
 * inefficient way (and tossing it) until timeout.
 */
serial_retval_t
readchar_flush(int timeout_ms)
{
	unsigned char c;
	int ret;
	serial_retval_t retval = SERIAL_RET_OK;

	if (timeout_ms == 0)
		timeout_ms = 1;

	/* Set initial timer */
	timer_timeout_set(timeout_ms);

	/*
	 * Loop over and keep reading characters until we hit timeout.
	 */
	while (1) {
		/*
		 * Don't wait here if the serial port is using QUICK
		 * and is ready.
		 */
		if (serial_read_is_ready() == 0) {
			Wait(serial_get_read_signal_bitmask() |
			    serial_get_abort_keypress_signal_bitmask() |
			    timer_get_signal_bitmask());
		}

		/* Check if we hit our timeout timer */
		if (timer_timeout_fired()) {
			timer_timeout_complete();
			break;
		}

		/* Try to read a character, but don't block */
		ret = serial_get_char(&c);
		if (ret == 0) {
			timer_timeout_abort();
			timer_timeout_set(timeout_ms);
		} else if (ret > 0) {
			/* Got a char */
			serial_read_start();
		} else if (ret < 0) {
			serial_read_start();
		}

		if (serial_read_check_keypress_fn() == true) {
			emits("\nUser cancelled transfer\n");
			retval = SERIAL_RET_ABORT;
			break;
		}
	}

	/* Not sure - do I need to do this in case it fired? */
	timer_timeout_abort();

	return (retval);
}
