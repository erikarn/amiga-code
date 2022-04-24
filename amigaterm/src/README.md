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
* The receive path uses bulk transfers rather than byte-at-a-time.
* Aborts seem to be handled better now.

# What's next?

* Optimise the send path to use bulk sends rather than byte-at-a-time.
* Since stuff is mostly refactored out now, I can try to build command
  line xmodem send/receive tools for exchanging files without requiring
  any other system libraries, which is good for bootstrapping things.

