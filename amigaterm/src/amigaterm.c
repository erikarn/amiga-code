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

#include "amigaterm_screen.h"
#include "amigaterm_serial.h"
#include "amigaterm_serial_read.h"
#include "../lib/timer/timer.h"
#include "amigaterm_util.h"
#include "amigaterm_xmodem.h"

void filename(char name[], int len); // AF
long filesize(void);               // Read a file size, or default to -1
int XMODEM_Read_File(char *file, long size); // AF
int XMODEM_Send_File(char *file); // AF
int current_baud;
#define DOS_REV 1

/* Enable serial hardware flow control */
#define ENABLE_HWFLOW 1

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

  screen_init();

#if ENABLE_HWFLOW
  if (! serial_init(9600, 1)) {
#else
  if (! serial_init(9600, 0)) {
#endif
    puts("couldn't init serial\n");
    screen_cleanup();
    exit(TRUE);
  }
  current_baud = 9600;

  if (! timer_init()) {
    puts("couldn't init timer\n");
    screen_cleanup();
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
      draw_cursor(AMIGATERM_SCREEN_CURSOR_PEN, false); // Note: no XOR here
      // XXX TODO: we need to track this and blank/XOR the cursor out if
      // we've drawn it here, or we'll end up with cursor artefacts everywhere!
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
          emits("AMIGA Term Copyright 1985 by Michael Mounier\n");
          emits("AMIGA Term Enhanced 2018-2021 by Roc Vall\xe8s Dom\xe8nech\n");
          emits("Contributors: Alexander Fritsch (2021)\n");
          emits("Contributors: Adrian Chadd (2022)\n");
          emits("More info: https://github.com/erikarn/amiga-code/amigaterm/\n");
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
        emit(12); // XXX hack, but hey
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
  screen_cleanup();
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
