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


#define INTUITION_REV 1
#define GRAPHICS_REV 1

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

int
screen_init(void)
{

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

  return (1);
}

void
screen_cleanup(void)
{
  CloseWindow(mywindow);
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

  /* XOR bits into raster */
  SetDrMd(mywindow->RPort, COMPLEMENT);

  /* Cursor */
  SetAPen(mywindow->RPort, 3);
  RectFill(mywindow->RPort, cx - 7, cy - 6, cx, cy + 1);
  SetAPen(mywindow->RPort, 1);

  /* 2 colours? Or planes? Into raster */
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

  /* Cursor */
  SetAPen(mywindow->RPort, 3);
  RectFill(mywindow->RPort, cx - 7, cy - 6, cx, cy + 1);
  SetAPen(mywindow->RPort, 1);
}
