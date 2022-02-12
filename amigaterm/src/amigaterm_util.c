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

#include <exec/types.h>           // for FALSE, TRUE, UBYTE, CONST_STRPTR

#include "amigaterm_util.h"

/*************************************************
 *  function to take raw key data and convert it
 *  into ascii chars
 **************************************************/
char toasc(USHORT code)
{
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
