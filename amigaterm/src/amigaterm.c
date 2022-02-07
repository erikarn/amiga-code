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
#include <devices/serial.h>       // for IOExtSer, SERF_SHARED, SERF_XDISA...
#include <exec/types.h>           // for FALSE, TRUE, UBYTE, CONST_STRPTR
#include <intuition/intuition.h>  // for MenuItem, IntuiText, Menu, Window
#include <intuition/screens.h>    // for RAWKEY, CLOSEWINDOW, MENUPICK
#include <stdio.h>                // for NULL, puts, fclose, fopen, EOF, getc
void sendchar(int ch);            // AF
void emits(char string[]);        // AF
void emit(char c);                // AF
void filename(char name[]);       // AF
int XMODEM_Read_File(char *file); // AF
int XMODEM_Send_File(char *file); // AF
#define DOS_REV 1
#define INTUITION_REV 1
#define GRAPHICS_REV 1
/* things for xmodem send and recieve */
#define SECSIZ 0x80
#define TTIME 30       /* number of seconds for timeout */
#define BufSize 0x1000 /* Text buffer */
#define ERRORMAX 10    /* Max errors before abort */
#define RETRYMAX 10    /* Maximum retrys before abort */
#define SOH 1          /* Start of sector char */
#define EOT 4          /* end of transmission char */
#define ACK 6          /* acknowledge sector transmission */
#define NAK 21         /* error in transmission detected */
static char bufr[BufSize];
static int timeout = FALSE;
static long bytes_xferred;
static BPTR fh;
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
static char toasc(USHORT code);  // AF
static unsigned char readchar();
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
/* declarations for the serial stuff */
extern struct MsgPort *CreatePort();
struct IOExtSer *Read_Request;
static char rs_in[2];
struct IOExtSer *Write_Request;
static char rs_out[2];
/******************************************************/
/*                   Main Program                     */
/*                                                    */
/*      This is the main body of the program.         */
/******************************************************/
int main() {
  ULONG class;
  USHORT code, menunum, itemnum;
  int KeepGoing, capture, send;
  char c, name[32];
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
  Read_Request = (struct IOExtSer *)AllocMem(sizeof(*Read_Request),
                                             MEMF_PUBLIC | MEMF_CLEAR);
  Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
  Read_Request->IOSer.io_Message.mn_ReplyPort =
      CreatePort((CONST_STRPTR) "Read_RS", 0);
  if (OpenDevice((CONST_STRPTR)SERIALNAME, 0, (struct IORequest *)Read_Request,
                 0)) {
    puts("Can't open Read device\n");
    CloseWindow(mywindow);
    DeletePort(Read_Request->IOSer.io_Message.mn_ReplyPort);
    FreeMem(Read_Request, sizeof(*Read_Request));
    exit(TRUE);
  }
  Read_Request->IOSer.io_Command = CMD_READ;
  Read_Request->IOSer.io_Length = 1;
  Read_Request->IOSer.io_Data = (APTR)&rs_in[0];
  Write_Request = (struct IOExtSer *)AllocMem(sizeof(*Write_Request),
                                              MEMF_PUBLIC | MEMF_CLEAR);
  Write_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
  Write_Request->IOSer.io_Message.mn_ReplyPort =
      CreatePort((CONST_STRPTR) "Write_RS", 0);
  if (OpenDevice((CONST_STRPTR)SERIALNAME, 0, (struct IORequest *)Write_Request,
                 0)) {
    puts("Can't open Write device\n");
    CloseWindow(mywindow);
    DeletePort(Write_Request->IOSer.io_Message.mn_ReplyPort);
    FreeMem(Write_Request, sizeof(*Write_Request));
    DeletePort(Read_Request->IOSer.io_Message.mn_ReplyPort);
    FreeMem(Read_Request, sizeof(*Read_Request));
    exit(TRUE);
  }
  Write_Request->IOSer.io_Command = CMD_WRITE;
  Write_Request->IOSer.io_Length = 1;
  Write_Request->IOSer.io_Data = (APTR)&rs_out[0];
  /*Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED | SERF_7WIRE;*/
  Read_Request->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
  Read_Request->io_Baud = 9600;
  Read_Request->io_ReadLen = 8;
  Read_Request->io_WriteLen = 8;
  Read_Request->io_CtlChar = 1L;
  Read_Request->IOSer.io_Command = SDCMD_SETPARAMS;
  DoIO((struct IORequest *)Read_Request);
  Read_Request->IOSer.io_Command = CMD_READ;
  InitFileItems();
  InitRSItems();
  InitMenu();
  SetMenuStrip(mywindow, &menu[0]);
  KeepGoing = TRUE;
  capture = FALSE;
  send = FALSE;
  SetAPen(mywindow->RPort, 1);
  emit(12);
  BeginIO((struct IORequest *)Read_Request);
  while (KeepGoing) {
    /* wait for window message or serial port message */
    Wait((1 << Read_Request->IOSer.io_Message.mn_ReplyPort->mp_SigBit) |
         (1 << mywindow->UserPort->mp_SigBit));
    if (send) {
      if ((c = getc(trans)) != EOF)
        sendchar(c);
      else {
        fclose(trans);
        emits("\nFile Sent\n");
        send = FALSE;
      }
    }
    if (CheckIO((struct IORequest *)Read_Request)) {
      WaitIO((struct IORequest *)Read_Request);
      c = rs_in[0] & 0x7f;
      BeginIO((struct IORequest *)Read_Request);
      emit(c);
      if (capture)
        if ((c > 31 && c < 127) || c == 10) /* trash them mangy ctl chars */
          putc(c, tranr);
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
          emits("***This program doesn't use flow control\n");
          emits("***ESC Aborts Xmodem Xfer\n");
          break;
        default:
          c = toasc(code); /* get in into ascii */
          if (c != 0) {
            rs_out[0] = c;
            DoIO((struct IORequest *)Write_Request);
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
                filename(name);
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
                filename(name);
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
              filename(name);
              if (XMODEM_Read_File(name)) {
                emits("Received\n");
                emit(8);
              } else {
                Close(fh);
                emits("Xmodem Receive Failed\n");
                emit(8);
              }
              break;
            case 3:
              emits("\nXmodem Send:");
              filename(name);
              if (XMODEM_Send_File(name)) {
                emits("Sent\n");
                emit(8);
              } else {
                Close(fh);
                emits("\nXmodem Send Failed\n");
                emit(8);
              }
              break;
            }
            break;
          case 1:
            AbortIO((struct IORequest *)Read_Request);
            switch (itemnum) {
            case 0:
              Read_Request->io_Baud = 300;
              break;
            case 1:
              Read_Request->io_Baud = 1200;
              break;
            case 2:
              Read_Request->io_Baud = 2400;
              break;
            case 3:
              Read_Request->io_Baud = 4800;
              break;
            case 4:
              Read_Request->io_Baud = 9600;
              break;
            case 5:
              Read_Request->io_Baud = 19200;
              break;
            case 6:
              Read_Request->io_Baud = 38400;
              break;
            case 7:
              Read_Request->io_Baud = 57600;
              break;
            case 8:
              Read_Request->io_Baud = 115200;
              break;
            }
            Read_Request->IOSer.io_Command = SDCMD_SETPARAMS;
            DoIO((struct IORequest *)Read_Request);
            Read_Request->IOSer.io_Command = CMD_READ;
            BeginIO((struct IORequest *)Read_Request);
            break;
          } /* end of switch ( menunum ) */
        }   /*  end of if ( not null ) */
      }     /* end of switch (class) */
    }       /* end of while ( newmessage )*/
  }         /* end while ( keepgoing ) */
  /*   It must be time to quit, so we have to clean
   *   up and exit.
   */
  CloseDevice((struct IORequest *)Read_Request);
  DeletePort(Read_Request->IOSer.io_Message.mn_ReplyPort);
  FreeMem(Read_Request, sizeof(*Read_Request));
  CloseDevice((struct IORequest *)Write_Request);
  DeletePort(Write_Request->IOSer.io_Message.mn_ReplyPort);
  FreeMem(Write_Request, sizeof(*Write_Request));
  ClearMenuStrip(mywindow);
  CloseWindow(mywindow);
  exit(FALSE);
} /* end of main */
/*************************************************
 *  function to get file name
 *************************************************/
void filename(char name[]) {
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
      if (class == RAWKEY) { // AF Achtung, Fehler! -->  ==
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
      }
    } /* end of new message loop */
  }   /* end of god knows what */
  emit(13);
} /* end of function */
/********************************/
/*  function to print a string */
/******************************/
void emits(char string[]) {
  int i;
  char c;
  i = 0;
  while (string[i] != 0) {
    c = string[i];
    if (c == 10)
      c = 13;
    emit(c);
    i += 1;
  }
}
/***************************************************************/
/*  send char and read char functions for the xmodem function */
/*************************************************************/
void sendchar(int ch) {
  rs_out[0] = ch;
  DoIO((struct IORequest *)Write_Request);
}
static unsigned char readchar() {
  unsigned char c;
  int rd, ch = 0;
  rd = FALSE;
  while (rd == FALSE) {
    Wait((1 << Read_Request->IOSer.io_Message.mn_ReplyPort->mp_SigBit) |
         (1 << mywindow->UserPort->mp_SigBit));
    if (CheckIO((struct IORequest *)Read_Request)) {
      WaitIO((struct IORequest *)Read_Request);
      ch = rs_in[0];
      rd = TRUE;
      BeginIO((struct IORequest *)Read_Request);
    }
    if ((NewMessage = (struct IntuiMessage *)GetMsg(mywindow->UserPort)))
      if ((NewMessage->Class) == RAWKEY)
        if ((NewMessage->Code) == 69) {
          emits("\nUser Cancelled Transfer");
          break;
        }
  }
  if (rd == FALSE) {
    timeout = TRUE;
    emits("\nTimeout Waiting For Character\n");
  }
  c = ch;
  return c;
}
/***************************************/
/*  xmodem send and receive functions */
/*************************************/
int XMODEM_Read_File(char *file) {
  int firstchar, sectnum, sectcurr, sectcomp, errors, errorflag;
  unsigned int checksum, j, bufptr;
  bytes_xferred = 0L;
  if ((fh = Open((UBYTE *)file, MODE_NEWFILE)) < 0) {
    emits("Cannot Open File\n");
    return FALSE;
  } else
    emits("Receiving File...");
  timeout = FALSE;
  sectnum = errors = bufptr = 0;
  sendchar(NAK);
  firstchar = 0;
  while (firstchar != EOT && errors != ERRORMAX) {
    errorflag = FALSE;
    do { /* get sync char */
      firstchar = readchar();
      if (timeout == TRUE)
        return FALSE;
    } while (firstchar != SOH && firstchar != EOT);
    if (firstchar == SOH) {
      /*emits("Getting Block ");*/
      /*stci_d(numb,sectnum,i);*/
      /*snprintf(numb, 10, "%u", sectnum);*/
      /*emits(numb);*/
      /*emits("...");*/
      /*emits("-");*/
      sectcurr = readchar();
      if (timeout == TRUE)
        return FALSE;
      sectcomp = readchar();
      if (timeout == TRUE)
        return FALSE;
      if ((sectcurr + sectcomp) == 255) {
        if (sectcurr == ((sectnum + 1) & 0xff)) {
          checksum = 0;
          for (j = bufptr; j < (bufptr + SECSIZ); j++) {
            bufr[j] = readchar();
            if (timeout == TRUE)
              return FALSE;
            checksum = (checksum + bufr[j]) & 0xff;
          }
          if (checksum == readchar()) {
            errors = 0;
            sectnum++;
            bufptr += SECSIZ;
            bytes_xferred += SECSIZ;
            /*emits("verified\n");*/
            /*emits("+");*/
            if (bufptr == BufSize) {
              bufptr = 0;
              if (Write(fh, bufr, BufSize) == EOF) {
                emits("\nError Writing File\n");
                return FALSE;
              };
            };
            sendchar(ACK);
          } else {
            errorflag = TRUE;
            if (timeout == TRUE)
              return FALSE;
          }
        } else {
          if (sectcurr == (sectnum & 0xff)) {
            emits("\nReceived Duplicate Sector\n");
            sendchar(ACK);
          } else
            errorflag = TRUE;
        }
      } else
        errorflag = TRUE;
    }
    if (errorflag == TRUE) {
      errors++;
      emits("\nError\n");
      sendchar(NAK);
    }
  }; /* end while */
  if ((firstchar == EOT) && (errors < ERRORMAX)) {
    sendchar(ACK);
    Write(fh, bufr, bufptr);
    Close(fh);
    return TRUE;
  }
  return FALSE;
}
int XMODEM_Send_File(char *file) {
  int sectnum, bytes_to_send, size, attempts, c;
  unsigned checksum, j, bufptr;
  timeout = FALSE;
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
  while (((c = readchar()) != NAK) && (j++ < ERRORMAX))
    ;
  if (j >= (ERRORMAX)) {
    emits("\nReceiver not sending NAKs\n");
    return FALSE;
  }
  while ((bytes_to_send = Read(fh, bufr, BufSize)) && attempts != RETRYMAX) {
    if (bytes_to_send == EOF) {
      emits("\nError Reading File\n");
      return FALSE;
    };
    bufptr = 0;
    while (bytes_to_send > 0 && attempts != RETRYMAX) {
      attempts = 0;
      do {
        sendchar(SOH);
        sendchar(sectnum);
        sendchar(~sectnum);
        checksum = 0;
        size = SECSIZ <= bytes_to_send ? SECSIZ : bytes_to_send;
        bytes_to_send -= size;
        for (j = bufptr; j < (bufptr + SECSIZ); j++)
          if (j < (bufptr + size)) {
            sendchar(bufr[j]);
            checksum += bufr[j];
          } else
            sendchar(0);
        sendchar(checksum & 0xff);
        attempts++;
        c = readchar();
        if (timeout == TRUE)
          return FALSE;
      } while ((c != ACK) && (attempts != RETRYMAX));
      bufptr += size;
      bytes_xferred += size;
      /* emits("Block "); */
      /* stci_d(numb,sectnum,i); */
      /* snprintf(numb, 10, "%u", sectnum); */
      /* emits(numb); */
      /* emits(" sent\n"); */
      sectnum++;
    }
  }
  Close(fh);
  if (attempts == RETRYMAX) {
    emits("\nNo Acknowledgment Of Sector, Aborting\n");
    return FALSE;
  } else {
    attempts = 0;
    do {
      sendchar(EOT);
      attempts++;
    } while ((readchar() != ACK) && (attempts != RETRYMAX) &&
             (timeout == FALSE));
    if (attempts == RETRYMAX)
      emits("\nNo Acknowledgment Of End Of File\n");
  };
  return TRUE;
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
/*************************************************
 *  function to take raw key data and convert it
 *  into ascii chars
 **************************************************/
static char toasc(USHORT code) {
  static int ctrl = FALSE;
  static int shift = FALSE;
  static int capsl = FALSE;
  char c;
  static char keys[75] = {
      '`',  '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-', '=',
      '\\', 0,    '0', 'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
      '[',  ']',  0,   '1',  '2', '3', 'a', 's', 'd', 'f', 'g', 'h', 'j',
      'k',  'l',  ';', '\'', 0,   0,   '4', '5', '6', 0,   'z', 'x', 'c',
      'v',  'b',  'n', 'm',  44,  '.', '/', 0,   '.', '7', '8', '9', ' ',
      8,    '\t', 13,  13,   27,  127, 0,   0,   0,   '-'};
  switch (
      code) { /* I didn't know about the Quilifier field when I write this */
  case 98:
    capsl = TRUE;
    c = 0;
    break;
  case 226:
    capsl = FALSE;
    c = 0;
    break;
  case 99:
    ctrl = TRUE;
    c = 0;
    break;
  case 227:
    ctrl = FALSE;
    c = 0;
    break;
  case 96:
  case 97:
    shift = TRUE;
    c = 0;
    break;
  case 224:
  case 225:
    shift = FALSE;
    c = 0;
    break;
  default:
    if (code < 75)
      c = keys[code];
    else
      c = 0;
  }
  /* add modifiers to the keys */
  if (c != 0) {
    if (ctrl && (c <= 'z') && (c >= 'a'))
      c -= 96;
    else if (shift) {
      if ((c <= 'z') && (c >= 'a'))
        c -= 32;
      else
        switch (c) {
        case '[':
          c = '{';
          break;
        case ']':
          c = '}';
          break;
        case '\\':
          c = '|';
          break;
        case '\'':
          c = '"';
          break;
        case ';':
          c = ':';
          break;
        case '/':
          c = '?';
          break;
        case '.':
          c = '>';
          break;
        case ',':
          c = '<';
          break;
        case '`':
          c = '~';
          break;
        case '=':
          c = '+';
          break;
        case '-':
          c = '_';
          break;
        case '1':
          c = '!';
          break;
        case '2':
          c = '@';
          break;
        case '3':
          c = '#';
          break;
        case '4':
          c = '$';
          break;
        case '5':
          c = '%';
          break;
        case '6':
          c = '^';
          break;
        case '7':
          c = '&';
          break;
        case '8':
          c = '*';
          break;
        case '9':
          c = '(';
          break;
        case '0':
          c = ')';
          break;
        default:; // AF
        }         /* end switch */
    }             /* end shift */
    else if (capsl && (c <= 'z') && (c >= 'a'))
      c -= 32;
  } /* end modifiers */
  return c;
} /* end of routine */
/* end of file */
