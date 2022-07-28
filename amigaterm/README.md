amigaterm
=========

This is a fork of amigaterm enhanced.  That's a fork of
the original amigaterm.

* https://aminet/package/comm/term/amigaterm
* https://aminet/package/comm/term/amigaterm_enh

I'm working on making it as correct and stable as possible.

There's a whole bunch of fixes here!

* Attempting to correctly close libraries and other resources when
  quitting.
* Use serial bulk transfers during xmodem send/receive; handle
  underflow/overflow/frame errors.
* Use a timer to detect if we're missing bytes during transfers and
  schedule a NAK.
* Specify the file size when receiving xmodem files so we don't
  have them rounded up to multiples of 128 bytes.
* Update the screen routines to use the font / window sizes from
  Intuition, rather than hard-coded sizes everywhere.

This is still a work in progress.  My hope is to have this be
an example of a well-written Amiga kickstart/workbench 1.3 application
with all the error handling implement, no memory / resource leaks,
correct / fast graphics and serial routines, etc.

I'd also like to eventually support an optional separate screen / ANSI
support build mode so it can be a more fully featured terminal program,
but maintaining the option to build a small footprint minimal terminal
useful for bootstrapping.

To build
--------

* Use the toolchain from https://github.com/bebbo/amiga-gcc
* Use GNU make in the source directory to build it!

Tested
------

I've tested this on Workbench 1.3 on an Amiga 500, and Workbench 3.1
on an Amiga 1200.
