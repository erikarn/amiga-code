Amigaterm hackery
=================

# What's this?

This is me poking at "correct" serial port handling, timer handling, etc
inside of AmigaOS 1.3.  The original amigaterm took a lot of shortcuts
on things and was both unstable and badly performing.

# What stuff is being poked at?

* I've actually implemented timers to abort IO, and figured out when
  and how stuff needs handling.
* Fixed a few message leaks around handling the abort key.
* The receive path uses bulk transfers rather than byte-at-a-time -
  it'll now receive around 1300 bytes/sec at 19200 baud running out
  of slow/chip ram on a stock Amiga 500.
* IO aborts/timeouts seem to be handled better now - if you try using too
  fast a baud rate then the transfer will eventually exit rather than
  cause the system to hang.
* ... and that includes actually /handling/ the receive overflow error
  from the serial driver!

# What's next?

* Optimise the send path to use bulk sends rather than byte-at-a-time.
* Since stuff is mostly refactored out now, I can try to build command
  line xmodem send/receive tools for exchanging files without requiring
  any other system libraries, which is good for bootstrapping things.

