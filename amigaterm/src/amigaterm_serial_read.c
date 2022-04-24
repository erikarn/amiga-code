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
extern int current_baud;

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

serial_retval_t
readchar_sched(int schedule_next, int timeout_ms, unsigned char *ch)
{
  unsigned char c = 0;
  int rd, ret;
  serial_retval_t retval = SERIAL_RET_OK;

  rd = FALSE;

  if (timeout_ms > 0) {
      timer_timeout_set(timeout_ms);
  }

  while (rd == FALSE) {
    /* Don't wait here if the serial port is using QUICK and is ready */
    if (serial_read_is_ready() == 0) {
        Wait(serial_get_read_signal_bitmask() |
             (serial_get_abort_keypress_signal_bitmask()) |
             (timer_get_signal_bitmask()));
    }
    ret = serial_get_char(&c);
    if (ret < 0) {
      /* IO error - we need to re-schedule another IO and break here */
      rd = FALSE;
      if (schedule_next) {
          serial_read_start();
      }
      c = 0;
      break;
    } else if (ret > 0) {
      rd = TRUE;
      if (schedule_next) {
          serial_read_start();
      }
      break;
    }

    /* Check if we hit our timeout timer */
    if (timer_timeout_fired()) {
        timer_timeout_complete();
//        emits("Timeout (readchar_sched)\n");
        rd = FALSE;
        retval = SERIAL_RET_TIMEOUT;
        break;
    }

    if (serial_read_check_keypress_fn() == true) {
      emits("User Cancelled Transfer\n");
      rd = FALSE;
      retval = SERIAL_RET_ABORT;
      break;
    }

  }

  // Abort any pending timer
  timer_timeout_abort();
  *ch = c;

  return retval;
}

serial_retval_t
readchar(unsigned char *ch)
{
    return readchar_sched(1, 1000, ch);
}

/*
 * Read 'len' bytes into the given buffer.
 *
 * Returns a serial_retval_t explaning if it's OK, timeout or aborted.
 *
 * This is a bit dirty for now because of how the current xmodem
 * receive path is written.  readchar() (a) expects a single byte
 * read is already scheduled, (b) will wait for it to complete,
 * then (c) dequeue it and start a new single byte read.
 *
 * Instead here, we'll do a single byte read first without scheduling
 * a follow-up single byte read.  Then we'll do a bigger read for
 * the rest.  Once that's done we'll kick-off another single byte
 * read.
 */
serial_retval_t
readchar_buf(char *buf, int len)
{
#if 0
    serial_retval_t retval;
    int i;
    char ch;

    for (i = 0; i < len; i++) {
      retval = readchar(&ch);
      if (retval != SERIAL_RET_OK)
          return retval;
      buf[i] = ch;
    }
    return SERIAL_RET_OK;
#else
    int rd, ret;
    serial_retval_t retval = SERIAL_RET_OK;
    unsigned char ch;
    int cur_timeout;

    /* First character, don't schedule the next read */
    /*
     * XXX TODO: is this actually correct?
     * like, could we have some race where both happens and
     * we never schedule a follow-up read?
     */
    retval = readchar_sched(0, 1000, &ch);
    if (retval != SERIAL_RET_OK) {
      goto done;
    }

    buf[0] = ch;

    /* If we don't have any other bytes, finish + schedule read */
    if (len == 1) {
        serial_read_start();
        return len;
    }

    /*
     * Timeout is 128 bytes at the baud rate; add 50% in case
     * and make sure it's at least a second, so we properly
     * have timed out.
     */
    if (current_baud == 0) {
        cur_timeout = 1000;
    } else {
        cur_timeout = (128 * 15 * 1000) / current_baud;
    }
    if (cur_timeout < 1000)
        cur_timeout = 1000;

//    printf("%s: Timeout: %d milliseconds\n", __func__, cur_timeout);

    if (cur_timeout > 0)
        timer_timeout_set(cur_timeout);

    /* Schedule a read for the rest */
    serial_read_start_buf(&buf[1], len - 1);

    /* Now we wait until it's completed or timeout */
    rd = FALSE;

    while (rd == FALSE) {
      /* Don't wait here if the serial port is using QUICK and is ready */
      if (serial_read_is_ready() == 0) {
          Wait(serial_get_read_signal_bitmask() |
               (serial_get_abort_keypress_signal_bitmask()) |
               timer_get_signal_bitmask());
      }

      /* Check if the serial IO is completed */
      if (serial_read_ready()) {
        /* Complete the read */
        ret = serial_read_wait();
        /* We've completed the read, so break here */
        if (ret == 1) {
           /* IO was OK */
           rd = TRUE;
        } else if (ret == 0) {
           /* No IO yet */
           rd = FALSE;
        } else {
           /* IO error */
           rd = FALSE;
        }
        break;
      }

      /* Check if timeout */
      if (timer_timeout_fired()) {
        timer_timeout_complete();
        serial_read_abort();
        rd = FALSE;
        retval = SERIAL_RET_TIMEOUT;
//        emits("Timeout (readchar_buf)\n");
        break;
      }

      /* Check for being cancelled */
      if (serial_read_check_keypress_fn() == true) {
        emits("User Cancelled Transfer\n");
        /* Abort the current IO */
        serial_read_abort();
        retval = SERIAL_RET_ABORT;
        break;
      }

    }

    /*
     * At this point we've either finished or aborted.
     * So I /hope/ it's safe to schedule the new single byte read.
     */
    serial_read_start();

    /*
     * And it's safe to abort this; the timer layer will only do
     * the IO abort if it was actually scheduled.
     */
    timer_timeout_abort();

    /*
     * Now return our status here.
     */
done:
    return retval;
#endif
}

