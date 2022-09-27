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
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

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

struct amigaterm_screen {
  short font_height, font_width, font_baseline; // pixels
  short tab_width; // characters
  short cursor_x, cursor_y; // current character x/y position
  short scr_width, scr_height; // current width/height in characters
};

struct amigaterm_screen a_screen;

/*
 * Get the cursor in the window pixel coordinates.
 */
static void
screen_get_cursor_xy(short *x, short *y)
{
	if (x != NULL) {
		*x = a_screen.cursor_x * a_screen.font_width;
		*x += mywindow->BorderLeft;
	}
	if (y != NULL) {
		*y = a_screen.cursor_y * a_screen.font_height;
		*y += mywindow->BorderTop;
	}
}

/*
 * Update the cursor position; do bounds check.
 */
static void
screen_set_cursor(short x, short y)
{
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	if (x >= a_screen.scr_width)
		x = a_screen.scr_width - 1;

	if (y >= a_screen.scr_height)
		y = a_screen.scr_height - 1;

	a_screen.cursor_x = x;
	a_screen.cursor_y = y;
}

/*
 * Advance the cursor.  Optionally wrap or stay at the end.
 *
 * Returns true if we need to scroll the screen up (ie, we
 * hit the last character on the last line, and we're not
 * wrapping.)
 */
static bool
screen_advance_cursor(short num_char, bool do_wrap)
{
	bool do_y = false;
	bool ret = false;

	/* Advance cursor, wrap at x */
	a_screen.cursor_x += num_char;
	if (a_screen.cursor_x >= a_screen.scr_width) {
		if (do_wrap) {
			a_screen.cursor_x %= a_screen.scr_width;
			do_y = true;
		} else {
			a_screen.cursor_x = a_screen.scr_width - 1;
		}
	}

	/* Check if we need to advance y now */
	if (do_y == true) {
		a_screen.cursor_y += 1;
		if (a_screen.cursor_y >= a_screen.scr_height) {
			a_screen.cursor_y = a_screen.scr_height - 1;
			ret = true;
		}
	}
	return ret;
}

/* Advance to next line; code dupe with above */
static bool
screen_advance_line(void)
{
	bool ret = 0;

	a_screen.cursor_x = 0;
	a_screen.cursor_y++;
	if (a_screen.cursor_y >= a_screen.scr_height) {
		a_screen.cursor_y = a_screen.scr_height - 1;
		ret = true;
	}

	return ret;
}

/*
 * Calculate the maximum screen dimensions for the text area.
 *
 * This needs to be re-calculated every time the screen is resized.
 */
static void
screen_init_dimensions(void)
{
	/* These are in pixels */
	a_screen.scr_width =
	    mywindow->Width - (mywindow->BorderLeft + mywindow->BorderRight);
	a_screen.scr_height =
	    mywindow->Height - (mywindow->BorderTop + mywindow->BorderBottom);

	/* Now convert to characters */
	a_screen.scr_width /= a_screen.font_width;
	a_screen.scr_height /= a_screen.font_height;

	printf("%s: width=%d, height=%d\n", __func__,
	    a_screen.scr_width, a_screen.scr_height);
}

/*
 * Read the system font parameters for the given window.
 */
static void
screen_read_system_font(void)
{

  a_screen.font_height = mywindow->RPort->Font->tf_YSize;
  a_screen.font_width = mywindow->RPort->Font->tf_XSize;
  a_screen.font_baseline = mywindow->RPort->Font->tf_Baseline;

  printf("%s: font: height %d width %d baseline %d\n",
    __func__, a_screen.font_height,
    a_screen.font_width,
    a_screen.font_baseline);
}

int
screen_init(void)
{

  IntuitionBase = (struct IntuitionBase *)OpenLibrary(
      (CONST_STRPTR) "intuition.library", INTUITION_REV);
  if (IntuitionBase == NULL) {
    puts("Can't open intuition library\n");
    goto error;
  }
  GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR) "graphics.library",
                                          GRAPHICS_REV);
  if (GfxBase == NULL) {
    puts("Can't open graphics library\n");
    goto error;
  }

  if ((mywindow = (struct Window *)OpenWindow(&NewWindow)) == NULL) {
    puts("Can't open window\n");
    goto error;
  }

  /*
   * Initialise the system font paramters.
   */
  screen_read_system_font();

  /* Default to 8 character tab */
  a_screen.tab_width = 8;

  a_screen.cursor_x = a_screen.cursor_y = 0;

  screen_init_dimensions();

  return (1);
error:
  if (GfxBase != NULL)
    CloseLibrary((struct Library *) GfxBase);
  if (IntuitionBase != NULL)
    CloseLibrary((struct Library *) IntuitionBase);
  exit(TRUE);
}

void
screen_cleanup(void)
{
  CloseWindow(mywindow);

  if (GfxBase != NULL)
    CloseLibrary((struct Library *) GfxBase);
  if (IntuitionBase != NULL)
    CloseLibrary((struct Library *) IntuitionBase);
}

/*
 * Draw the cursor at the current cursor location.
 */
void
draw_cursor(char pen, bool do_xor)
{
	short cx, cy;

	screen_get_cursor_xy(&cx, &cy);
	if (do_xor) {
		SetDrMd(mywindow->RPort, COMPLEMENT);
	}

	SetAPen(mywindow->RPort, pen);
	RectFill(mywindow->RPort, cx, cy,
	    cx + a_screen.font_width - 1, cy + a_screen.font_height - 1);
	SetAPen(mywindow->RPort, AMIGATERM_SCREEN_TEXT_PEN);

	if (do_xor) {
		SetDrMd(mywindow->RPort, JAM2);
	}

}

/*
 * Draw a character at the current location.  Don't advance the
 * cursor.
 */
static void
screen_draw_char(char c)
{
	short cx, cy;

	screen_get_cursor_xy(&cx, &cy);

	Move(mywindow->RPort, cx, cy + a_screen.font_baseline);

	Text(mywindow->RPort, (UBYTE *)&c, 1);
}

/*
 * Display an ASCII character and do basic terminal emulation.
 *
 * This doesn't draw the cursor, but it does update the
 * current cursor position for when it's time to update the
 * cursor.
 */
static void
_emit(char c)
{
  short xmax, ymax;

  bool do_scroll = false;

  xmax = mywindow->Width;
  ymax = mywindow->Height;

  switch (c) {
  case '\t':
    screen_advance_cursor(8, false); // tabstop 8, don't advance lines */
    break;
  case '\n':
    break;
  case 13: /* newline */
    do_scroll = screen_advance_line();
    break;
  case 8: /* backspace */
    screen_set_cursor(a_screen.cursor_x - 1, a_screen.cursor_y);
    break;
  case 12: /* page, also newsize message, so read the config */
    screen_read_system_font();
    screen_init_dimensions();
    screen_set_cursor(0, 0);

    /*
     * Blank the screen; note that I THINK this makes hard-coded assumptions
     * about the menu and window decoration size!
     *
     * XXX TODO: remove hard-coded assumptions!
     */
    SetAPen(mywindow->RPort, AMIGATERM_SCREEN_BACKGROUND_PEN);
    RectFill(mywindow->RPort, 2, 10, xmax - 19, ymax - 7);
    SetAPen(mywindow->RPort, AMIGATERM_SCREEN_TEXT_PEN);

    break;
  case 7: /* bell - flash the screen */
    ClipBlit(mywindow->RPort, 0, 0, mywindow->RPort, 0, 0, xmax, ymax, 0x50);
    ClipBlit(mywindow->RPort, 0, 0, mywindow->RPort, 0, 0, xmax, ymax, 0x50);
    break;
  default:
    /* Write the character; advance screen position */
    screen_draw_char(c);
    do_scroll = screen_advance_cursor(1, true); /* next line if needed */
    break;
  } /* end of switch */

  /*
   * if the cursor is off screen then next line; scroll if needed.
   *
   * Note that for some reason it's not wrapping screen_x here
   * but it IS wrapping the cursor position; the next trip through
   * _emit() will wrap the screen x/y position and draw the XOR
   * cursor in the next location.
   */
  if (do_scroll) {
    /* XXX again, hard-coded */
    ScrollRaster(mywindow->RPort, 0, 8, 2, 10, xmax - 20, ymax - 2);
  }
}

/*
 * Echo a single character.
 *
 * Updates the cursor each character.
 */
void
emit(char c)
{

  /* Clear cursor */
//  draw_cursor(AMIGATERM_SCREEN_BACKGROUND_PEN, false);

  /* Normal plotting - foreground + background */
  SetDrMd(mywindow->RPort, JAM2);
  _emit(c);

  /* draw cursor */
//  draw_cursor(AMIGATERM_SCREEN_CURSOR_PEN, false);
}

/*
 * Echo a string.
 *
 * Only emit a cursor update before and after the string
 * is echoed.
 */
void
emits(const char *str)
{
  int i;
  char c;
  i = 0;

  /* XOR cursor */
  //SetDrMd(mywindow->RPort, COMPLEMENT);
  draw_cursor(AMIGATERM_SCREEN_BACKGROUND_PEN, false);

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
  draw_cursor(AMIGATERM_SCREEN_CURSOR_PEN, false);
}
