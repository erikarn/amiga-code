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

static short screen_x = 3;
static short screen_y = 17;

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

static void
draw_cursor(bool do_xor)
{
	short cx, cy;
	const short xmax = mywindow->Width;
	const short ymax = mywindow->Height;

	if (screen_x > xmax) {
		return;
	}
	if (screen_y > ymax) {
		return;
	}

	/* cursor - at next position? Why, this seems just annoying */
	if (screen_x > (xmax - 31)) {
		cx = 9;
		cy = screen_y + 8;
	} else {
		cx = screen_x + 8;
		cy = screen_y;
	}
	if (cy > (ymax - 2)) {
		cx = 9;
		cy -= 8;
	}

	if (do_xor) {
		/* XOR bits into raster, so we can see "through" the cursor */
		SetDrMd(mywindow->RPort, COMPLEMENT);
	}

	/* Cursor */
	SetAPen(mywindow->RPort, 3);
	RectFill(mywindow->RPort, cx - 7, cy - 6, cx, cy + 1);
	SetAPen(mywindow->RPort, 1);

	if (do_xor) {
		/* Draw both foreground/background colours into raster */
		SetDrMd(mywindow->RPort, JAM2);
	}
}

void
screen_wrap_line(void)
{
	const short xmax = mywindow->Width;
	const short ymax = mywindow->Height;

	/* Handle wrapping at edge of window */
	if (screen_x > (xmax - 31)) {
		screen_x = 3;
		screen_y += 8;
	}
	if (screen_y > (ymax - 2)) {
		screen_x = 3;
		screen_y -= 8;
	}
}

/*
 * Display ASCII characters, don't draw the cursor.
 */
static void _emit(char c) {
  short xmax, ymax;
  short cx, cy;

  xmax = mywindow->Width;
  ymax = mywindow->Height;

  /* Update the current screen x/y if need to wrap */
  screen_wrap_line();

  Move(mywindow->RPort, screen_x, screen_y);

  switch (c) {
  case '\t':
    screen_x += 60;
    break;
  case '\n':
    break;
  case 13: /* newline */
    screen_x = 3;
    screen_y += 8;
    break;
  case 8: /* backspace */
    screen_x -= 8;
    if (screen_x < 3)
      screen_x = 3;
    break;
  case 12: /* page */
    screen_x = 3;
    screen_y = 17;

    /*
     * Blank the screen; note that I THINK this makes hard-coded assumptions
     * about the menu and window decoration size!
     */
    SetAPen(mywindow->RPort, 0);
    RectFill(mywindow->RPort, 2, 10, xmax - 19, ymax - 7);
    SetAPen(mywindow->RPort, 1);

    break;
  case 7: /* bell - flash the screen */
    ClipBlit(mywindow->RPort, 0, 0, mywindow->RPort, 0, 0, xmax, ymax, 0x50);
    ClipBlit(mywindow->RPort, 0, 0, mywindow->RPort, 0, 0, xmax, ymax, 0x50);
    break;
  default:
    /* Write the character; advance screen position */
    Text(mywindow->RPort, (UBYTE *)&c, 1);
    screen_x += 8;
  } /* end of switch */

  /*
   * if the cursor is off screen then next line; scroll if needed.
   *
   * Note that for some reason it's not wrapping screen_x here
   * but it IS wrapping the cursor position; the next trip through
   * _emit() will wrap the screen x/y position and draw the XOR
   * cursor in the next location.
   */
  if (screen_x > (xmax - 31)) {
    cx = 9;
    cy = screen_y + 8;
  } else {
    cx = screen_x + 8;
    cy = screen_y;
  }
  if (cy > (ymax - 2)) {
    cx = 9;
    cy -= 8;
    /* Scroll! */
    ScrollRaster(mywindow->RPort, 0, 8, 2, 10, xmax - 20, ymax - 2);
  }

  (void) cx; (void) cy;

  /* Wrap current x/y if required */
  screen_wrap_line();

}

/*
 * Echo a single character.
 */
void
emit(char c)
{
  /* XOR cursor */
  draw_cursor(true);

  /* Normal plotting - foreground + background */
  SetDrMd(mywindow->RPort, JAM2);
  _emit(c);

  /* draw cursor */
  draw_cursor(false);
}

/*
 * Echo a string.
 */
void
emits(const char *str)
{
  int i;
  char c;
  i = 0;

  /* XOR cursor */
  draw_cursor(true);

  /* Normal plotting - foreground + background */
  SetDrMd(mywindow->RPort, JAM2);

  while (str[i] != 0) {
    c = str[i];
    if (c == 10)
      c = 13;
    _emit(c);
    i += 1;
  }

  /* draw cursor */
  draw_cursor(false);
}
