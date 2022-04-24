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
#include "graphics/rastport.h"    // for JAM2, COMPLEMENT
#include "intuition/screens.h"    // for WBENCHSCREEN
#include "proto/dos.h"            // for Close, Open, Write, Read
#include "proto/exec.h"           // for FreeMem, DoIO, GetMsg, AllocMem
#include "proto/graphics.h"       // for SetAPen, RectFill, ClipBlit, SetDrMd
#include "proto/intuition.h"      // for CloseWindow, ClearMenuStrip, Open...
#include "stdlib.h"               // for exit
#include <clib/alib_protos.h>     // for DeletePort, BeginIO
#include <exec/types.h>           // for FALSE, TRUE, UBYTE, CONST_STRPTR
#include <intuition/intuition.h>  // for MenuItem, IntuiText, Menu, Window
#include <intuition/screens.h>    // for RAWKEY, CLOSEWINDOW, MENUPICK
#include <stdio.h>                // for NULL, puts, fclose, fopen, EOF, getc
#include <stdbool.h>

#include "amigaterm_serial.h"
#include "amigaterm_serial_read.h"
#include "amigaterm_timer.h"
#include "amigaterm_util.h"
#include "amigaterm_xmodem.h"

void emits(const char *str); // AF
void emit(char c);                // AF
void filename(char name[], int len); // AF
long filesize(void);               // Read a file size, or default to -1
int XMODEM_Read_File(char *file, long size); // AF
int XMODEM_Send_File(char *file); // AF
int current_baud;
#define DOS_REV 1
#define INTUITION_REV 1
#define GRAPHICS_REV 1

/* things for xmodem send and recieve */
#define TTIME 30       /* number of seconds for timeout */
#define BufSize 0x1000 /* Text buffer */
#define ERRORMAX 10    /* Max errors before abort */
#define RETRYMAX 10    /* Maximum retrys before abort */

/* Enable serial hardware flow control */
#define ENABLE_HWFLOW 1

/*   Intuition always wants to see these declarations */
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
/* my window structure */
struct NewWindow NewWindow = {
    0,
    0,
    640,
    200,
    0,
    1,
    CLOSEWINDOW | RAWKEY | MENUPICK | NEWSIZE,
    WINDOWCLOSE | SMART_REFRESH | ACTIVATE | WINDOWDRAG | WINDOWDEPTH |
        WINDOWSIZING | REPORTMOUSE,
    NULL,
    NULL,
    (UBYTE *)"AMIGA Terminal Enhanced",
    NULL,
    NULL,
    100,
    35,
    640,
    200,
    WBENCHSCREEN,
};
struct Window *mywindow;         /* ptr to applications window */
struct IntuiMessage *NewMessage; /* msg structure for GetMsg() */
/*****************************************************
 *                     File Menu
 *****************************************************/
/* define maximum number of menu items */
#define FILEMAX 4
/*   declare storage space for menu items and
 *   their associated IntuiText structures
 */
struct MenuItem FileItem[FILEMAX];
struct IntuiText FileText[FILEMAX];
/*****************************************************************/
/*    The following function initializes the structure arrays    */
/*   needed to provide the File menu topic.                      */
/*****************************************************************/
int InitFileItems() {
  short n;
  /* initialize each menu item and IntuiText with loop */
  for (n = 0; n < FILEMAX; n++) {
    FileItem[n].NextItem = &FileItem[n + 1];
    FileItem[n].LeftEdge = 0;
    FileItem[n].TopEdge = 11 * n;
    FileItem[n].Width = 135;
    FileItem[n].Height = 11;
    FileItem[n].Flags = ITEMTEXT | ITEMENABLED | HIGHBOX;
    FileItem[n].MutualExclude = 0;
    FileItem[n].ItemFill = (APTR)&FileText[n];
    FileItem[n].SelectFill = NULL;
    FileItem[n].Command = 0;
    FileItem[n].SubItem = NULL;
    FileItem[n].NextSelect = 0;
    FileText[n].FrontPen = 0;
    FileText[n].BackPen = 1;
    FileText[n].DrawMode = JAM2; /* render in fore and background */
    FileText[n].LeftEdge = 0;
    FileText[n].TopEdge = 1;
    FileText[n].ITextFont = NULL;
    FileText[n].NextText = NULL;
  }
  FileItem[FILEMAX - 1].NextItem = NULL;
  /* initialize text for specific menu items */
  FileText[0].IText = (UBYTE *)"Ascii Capture";
  FileText[1].IText = (UBYTE *)"Ascii Send";
  FileText[2].IText = (UBYTE *)"Xmodem Receive";
  FileText[3].IText = (UBYTE *)"Xmodem Send";
  return 0;
}
/*****************************************************/
/*                BaudRate  Menu                     */
/*****************************************************/
/* define maximum number of menu items */
#define RSMAX 9
/*   declare storage space for menu items and
 *   their associated IntuiText structures
 */
struct MenuItem RSItem[RSMAX];
struct IntuiText RSText[RSMAX];
/*****************************************************************/
/*    The following function initializes the structure arrays    */
/*   needed to provide the BaudRate menu topic.                  */
/*****************************************************************/
int InitRSItems() {
  short n;
  /* initialize each menu item and IntuiText with loop */
  for (n = 0; n < RSMAX; n++) {
    RSItem[n].NextItem = &RSItem[n + 1];
    RSItem[n].LeftEdge = 0;
    RSItem[n].TopEdge = 11 * n;
    RSItem[n].Width = 85;
    RSItem[n].Height = 11;
    RSItem[n].Flags = ITEMTEXT | ITEMENABLED | HIGHBOX | CHECKIT;
    RSItem[n].MutualExclude = (~(1 << n));
    RSItem[n].ItemFill = (APTR)&RSText[n];
    RSItem[n].SelectFill = NULL;
    RSItem[n].Command = 0;
    RSItem[n].SubItem = NULL;
    RSItem[n].NextSelect = 0;
    RSText[n].FrontPen = 0;
    RSText[n].BackPen = 1;
    RSText[n].DrawMode = JAM2; /* render in fore and background */
    RSText[n].LeftEdge = 0;
    RSText[n].TopEdge = 1;
    RSText[n].ITextFont = NULL;
    RSText[n].NextText = NULL;
  }
  RSItem[RSMAX - 1].NextItem = NULL;
  /* 9600 baud item checked */
  RSItem[4].Flags = ITEMTEXT | ITEMENABLED | HIGHBOX | CHECKIT | CHECKED;
  /* initialize text for specific menu items */
  RSText[0].IText = (UBYTE *)"   300";
  RSText[1].IText = (UBYTE *)"   1200";
  RSText[2].IText = (UBYTE *)"   2400";
  RSText[3].IText = (UBYTE *)"   4800";
  RSText[4].IText = (UBYTE *)"   9600";
  RSText[5].IText = (UBYTE *)"   19200";
  RSText[6].IText = (UBYTE *)"   38400";
  RSText[7].IText = (UBYTE *)"   57600";
  RSText[8].IText = (UBYTE *)"   115200";
  return 0;
}
/***************************************************/
/*                Menu Definition                  */
/*                                                 */
/*      This section of code is where the simple   */
/*   menu definition goes.                         */
/***************************************************/
/* current number of available menu topics */
#define MAXMENU 2
/*   declaration of menu structure array for
 *   number of current topics.  Intuition
 *   will use the address of this array to
 *   set and clear the menus associated with
 *   the window.
 */
struct Menu menu[MAXMENU];
/**********************************************************************/
/*   The following function initializes the Menu structure array with */
/*  appropriate values for our simple menu strip.  Review the manual  */
/*  if you need to know what each value means.                        */
/**********************************************************************/
int InitMenu() {
  menu[0].NextMenu = &menu[1];
  menu[0].LeftEdge = 5;
  menu[0].TopEdge = 0;
  menu[0].Width = 50;
  menu[0].Height = 10;
  menu[0].Flags = MENUENABLED;
  menu[0].MenuName = (BYTE *)"File"; /* text for menu-bar display */
  menu[0].FirstItem = &FileItem[0];  /* pointer to first item in list */
  menu[1].NextMenu = NULL;
  menu[1].LeftEdge = 65;
  menu[1].TopEdge = 0;
  menu[1].Width = 85;
  menu[1].Height = 10;
  menu[1].Flags = MENUENABLED;
  menu[1].MenuName = (BYTE *)"BaudRate"; /* text for menu-bar display */
  menu[1].FirstItem = &RSItem[0];        /* pointer to first item in list */
  return 0;
}

/******************************************************/
/*                   Main Program                     */
/*                                                    */
/*      This is the main body of the program.         */
/******************************************************/
int main() {
  ULONG class;
  USHORT code, menunum, itemnum;
  int KeepGoing, capture, send, baud, ret;
  char name[32];
  unsigned char c;
  long file_size;
  FILE *tranr = NULL;
  FILE *trans = NULL;
  IntuitionBase = (struct IntuitionBase *)OpenLibrary(
      (CONST_STRPTR) "intuition.library", INTUITION_REV);
  if (IntuitionBase == NULL) {
    puts("Can't open intuition library\n");
    exit(TRUE);
  }
  GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR) "graphics.library",
                                          GRAPHICS_REV);
  if (GfxBase == NULL) {
    puts("Can't open graphics library\n");
    exit(TRUE);
  }
  if ((mywindow = (struct Window *)OpenWindow(&NewWindow)) == NULL) {
    puts("Can't open window\n");
    exit(TRUE);
  }

#if ENABLE_HWFLOW
  if (! serial_init(9600, 1)) {
#else
  if (! serial_init(9600, 0)) {
#endif
    puts("couldn't init serial\n");
    CloseWindow(mywindow);
    exit(TRUE);
  }
  current_baud = 9600;

  if (! timer_init()) {
    puts("couldn't init timer\n");
    CloseWindow(mywindow);
    serial_close();
    exit(TRUE);
  }

  InitFileItems();
  InitRSItems();
  InitMenu();
  SetMenuStrip(mywindow, &menu[0]);
  KeepGoing = TRUE;
  capture = FALSE;
  send = FALSE;
  SetAPen(mywindow->RPort, 1);
  emit(12);

  /* Start a single byte serial read */
  serial_read_start();

  while (KeepGoing) {
    /*
     * wait for window message or serial port message
     *
     * if we are using QUICK IO then we can be already ready
     * to read, but we'll get no notification signal.
     * So skip the Wait().
     */
    if (serial_read_is_ready() == 0) {
      Wait((serial_get_read_signal_bitmask()) |
           (1 << mywindow->UserPort->mp_SigBit));
    }
    if (send) {
      if ((c = getc(trans)) != EOF)
        serial_write_char(c);
      else {
        fclose(trans);
        emits("\nFile Sent\n");
        send = FALSE;
      }
    }

    /* See if we have a serial read IO ready */
    ret = serial_get_char(&c);
    if (ret > 0) {
        /* Start another serial port read */
        serial_read_start();

        c = c & 0x7f;
        emit(c);
        if (capture) {
          if ((c > 31 && c < 127) || c == 10) /* trash them mangy ctl chars */
            putc(c, tranr);
        }
    } else if (ret < 0) {
        /* error (eg overflow) - need to re-queue serial read */
        serial_read_start();
    }

    while ((NewMessage = (struct IntuiMessage *)GetMsg(mywindow->UserPort))) {
      class = NewMessage->Class;
      code = NewMessage->Code;
      ReplyMsg((struct Message *)NewMessage);
      switch (class) {
      case CLOSEWINDOW:
        /*   User is ready to quit, so indicate
         *   that execution should terminate
         *   with next iteration of the loop.
         */
        KeepGoing = FALSE;
        break;
      case RAWKEY:
        /*  User has touched the keyboard */
        switch (code) {
        case 95: /* help key */
          emits("AMIGA Term Enhanced 2018-2021 by Roc Vall\xe8s Dom\xe8nech\n");
          emits("AMIGA Term Copyright 1985 by Michael Mounier\n");
          emits("Contributors: Alexander Fritsch (2021)\n");
#if ENABLE_HWFLOW
          emits("***This program is configured to USE HW flow control\n");
#else
          emits("***This program doesn't use flow control\n");
#endif
          emits("***ESC Aborts Xmodem Xfer\n");
          break;
        default:
          c = toasc(code); /* get in into ascii */
          if (c != 0) {
            serial_write_char(c);
          }
          break;
        }
        break;
      case NEWSIZE:
        emit(12);
        break;
      case MENUPICK:
        if (code != MENUNULL) {
          menunum = MENUNUM(code);
          itemnum = ITEMNUM(code);
          switch (menunum) {
          case 0:
            switch (itemnum) {
            case 0:
              if (capture == TRUE) {
                capture = FALSE;
                fclose(tranr);
                emits("\nEnd File Capture\n");
              } else {
                emits("\nAscii Capture:");
                filename(name, 31);
                if ((tranr = fopen(name, "w")) == 0) {
                  capture = FALSE;
                  emits("\nError Opening File\n");
                  break;
                }
                capture = TRUE;
              }
              break;
            case 1:
              if (send == TRUE) {
                send = FALSE;
                fclose(trans);
                emits("\nFile Send Cancelled\n");
              } else {
                emits("\nAscii Send:");
                filename(name, 31);
                if ((trans = fopen(name, "r")) == 0) {
                  send = FALSE;
                  emits("\nError Opening File\n");
                  break;
                }
                send = TRUE;
              }
              break;
            case 2:
              emits("\nXmodem Receive:");
              filename(name, 31);
              emits("\nFile size (or leave blank to not truncate):");
              file_size = filesize();

              /*
               * Ok, it's time to clean up how this receive path
               * works, and let's /begin/ by refactoring it so
               * it's cleaner and will close its own damned file
               * handle.
               */
              if (XMODEM_Read_File(name, file_size)) {
                emits("Received\n");
                emit(8);
              } else {
                emits("Xmodem Receive Failed\n");
                emit(8);
              }
              break;
            case 3:
              emits("\nXmodem Send:");
              filename(name, 31);
              if (XMODEM_Send_File(name)) {
                emits("Sent\n");
                emit(8);
              } else {
                emits("\nXmodem Send Failed\n");
                emit(8);
              }
              break;
            }
            break;
          case 1: /* Set baud rate */
            /* XXX TODO: why not just make this a table? */
            switch (itemnum) {
            case 0:
              baud = 300;
              break;
            case 1:
              baud = 1200;
              break;
            case 2:
              baud = 2400;
              break;
            case 3:
              baud = 4800;
              break;
            case 4:
              baud = 9600;
              break;
            case 5:
              baud = 19200;
              break;
            case 6:
              baud = 38400;
              break;
            case 7:
              baud = 57600;
              break;
            case 8:
              baud = 115200;
              break;
            default:
              baud = 300; /* XXX */
              break;
            }

            /* Abort the pending read IO, then set the serial baud */
            serial_read_abort();
            serial_set_baud(baud);
            current_baud = baud;

            /* Start a new read IO */
            serial_read_start();
            break;
          } /* end of switch ( menunum ) */
        }   /*  end of if ( not null ) */
      }     /* end of switch (class) */
    }       /* end of while ( newmessage )*/
  }         /* end while ( keepgoing ) */

  /*   It must be time to quit, so we have to clean
   *   up and exit.
   */
  serial_close();
  timer_close();
  ClearMenuStrip(mywindow);
  CloseWindow(mywindow);
  exit(FALSE);
} /* end of main */
/*************************************************
 *  function to get file name
 *************************************************/
void filename(char name[], int len) {
  char c;
  ULONG class;
  USHORT code;
  int keepgoing, i;
  keepgoing = TRUE;
  i = 0;
  while (keepgoing) {
    while ((NewMessage = (struct IntuiMessage *)GetMsg(mywindow->UserPort))) {
      class = NewMessage->Class;
      code = NewMessage->Code;
      ReplyMsg((struct Message *)NewMessage);
      if ((i < len) && (class == RAWKEY)) { // AF Achtung, Fehler! -->  ==
        c = toasc(code);
        name[i] = c;
        if (name[i] != 0) {
          if (name[i] == 13) {
            name[i] = 0;
            keepgoing = FALSE;
          } else {
            if (name[i] == 8) {
              i -= 2;
              if (i < -1)
                i = -1;
              else {
                emit(8);
                emit(32);
                emit(8);
              }
            } else
              emit(c);
          }
          i += 1;
        }
      } /* end of RAWKEY check */
    } /* end of new message loop */
  }   /* end of god knows what */
  emit(13);
} /* end of function */

/* Function to get a file size, or -1 if enter is pressed */
long filesize(void)
{
  char name[32];
  long val;

  filename(name, 31);
  if ((name[0] == 0) || (name[0] == 13))
    return -1L;
  val = strtol(name, NULL, 10);
  return (val);
}

/********************************/
/*  function to print a string */
/******************************/
void
emits(const char *str)
{
  int i;
  char c;
  i = 0;

  while (str[i] != 0) {
    c = str[i];
    if (c == 10)
      c = 13;
    emit(c);
    i += 1;
  }
}

bool
serial_read_check_keypress_fn(void)
{
  struct IntuiMessage *msg;
  bool retval = false;

  msg = (struct IntuiMessage *) GetMsg(mywindow->UserPort);
  if (msg == NULL)
    return false;

  if ((msg->Class) == RAWKEY) {
    if ((msg->Code) == 69) {
      retval = true;
    }
  }

  ReplyMsg((struct Message *)msg);

  return retval;
}

unsigned int
serial_get_abort_keypress_signal_bitmask(void)
{
  return (1 << mywindow->UserPort->mp_SigBit);
}


/***************************************************************/
/*  send char and read char functions for the xmodem function */
/*************************************************************/

static serial_retval_t
readchar_sched(int schedule_next, int timeout_ms, unsigned char *ch) {
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

/*************************************************
 *  function to output ascii chars to window
 *************************************************/
void emit(char c) {
  static short x = 3;
  static short y = 17;
  short xmax, ymax, cx, cy;
  xmax = mywindow->Width;
  ymax = mywindow->Height;
  /* cursor */
  if (x > (xmax - 31)) {
    cx = 9;
    cy = y + 8;
  } else {
    cx = x + 8;
    cy = y;
  }
  if (cy > (ymax - 2)) {
    cx = 9;
    cy -= 8;
  }
  SetDrMd(mywindow->RPort, COMPLEMENT);
  SetAPen(mywindow->RPort, 3);
  RectFill(mywindow->RPort, cx - 7, cy - 6, cx, cy + 1);
  SetAPen(mywindow->RPort, 1);
  SetDrMd(mywindow->RPort, JAM2);
  if (x > (xmax - 31)) {
    x = 3;
    y += 8;
  }
  if (y > (ymax - 2)) {
    x = 3;
    y -= 8;
  }
  Move(mywindow->RPort, x, y);
  switch (c) {
  case '\t':
    x += 60;
    break;
  case '\n':
    break;
  case 13: /* newline */
    x = 3;
    y += 8;
    break;
  case 8: /* backspace */
    x -= 8;
    if (x < 3)
      x = 3;
    break;
  case 12: /* page */
    x = 3;
    y = 17;
    SetAPen(mywindow->RPort, 0);
    RectFill(mywindow->RPort, 2, 10, xmax - 19, ymax - 7);
    SetAPen(mywindow->RPort, 1);
    break;
  case 7: /* bell */
    ClipBlit(mywindow->RPort, 0, 0, mywindow->RPort, 0, 0, xmax, ymax, 0x50);
    ClipBlit(mywindow->RPort, 0, 0, mywindow->RPort, 0, 0, xmax, ymax, 0x50);
    break;
  default:
    Text(mywindow->RPort, (UBYTE *)&c, 1);
    x += 8;
  } /* end of switch */
  /* cursor */
  if (x > (xmax - 31)) {
    cx = 9;
    cy = y + 8;
  } else {
    cx = x + 8;
    cy = y;
  }
  if (cy > (ymax - 2)) {
    cx = 9;
    cy -= 8;
    ScrollRaster(mywindow->RPort, 0, 8, 2, 10, xmax - 20, ymax - 2);
  }
  SetAPen(mywindow->RPort, 3);
  RectFill(mywindow->RPort, cx - 7, cy - 6, cx, cy + 1);
  SetAPen(mywindow->RPort, 1);
}
