ALLRAMBBS
=========

1983 cassette BBS ported from BASIC ;-)

This code represents a literal translation of 1983 Microsoft EXTENDED COLOR
BASIC to Arduino C. It should be the worst C code you will ever see...

See my original articles:

http://subethasoftware.com/2013/04/03/extended-color-basic-to-arduino-sketch/


REVISION
========
* 2013-04-02 0.0 allenh - Initial version with working userlog.
* 2013-04-03 1.0 allenh - Message base working. Core system fully functional.
                 Preliminary support for Arduino Ethernet added.
* 2013-04-04 1.1 allenh - SD card support for loading/saving.
* 2013-04-05 1.2 allenh - Ethernet (telnet) support.
* 2013-04-06 1.3 allenh - Cleanup for posting to www.appleause.com
* 2013-04-09 1.4 allenh - Fixed a bug with INPUT_SIZE and input.
* 2013-04-12 1.5 allenh - Integration with new Telnet server code, misc fixes.
* 2015-02-14 1.6 allenh - Adding some "const" to make it build with 1.6.0.
* 2015-02-14 1.6 allenh - Adding some "const" to make it build with 1.6.0.


FILES
=====

* README.md - this file
* ALLRAMBBS.ino - the main BBS.
* FlashMem.h - header file for flash memory stuff.
* sesTelnetServer.ino - my telnet server code.
* sesTelnetServerConfig.h - configuration stuff for telnet.

CONFIGURATION
=============

There are some #defines in ALLRAMBBS.ino that can be changed based on how much RAM
your Arduino has. The defaults are there for an UNO. There isn't really enough RAM
on an UNO to make this practical.

````
/*---------------------------------------------------------------------------*/
// In BASIC, strings are dynamic. For C, we have to pre-allocate buffers for
// the strings.
#define INPUT_SIZE  32  // 64. For aStr, etc.

#define MAX_USERS   3   // NM$(200) Userlog size. (0-200, 0 is Sysop)
#define NAME_SIZE   12  // 20. Username size (nmStr)
#define PSWD_SIZE   8   // 8. Password size (psStr & pwStr) 

...

// The original BASIC version was hard-coded to hold 20 messages of 11 lines
// each (the first line was used for From/To/Subject). The Arduino has far
// less RAM, so these have been made #defines so they can be changed.
#define MAX_MSGS    3   // 19  (0-19, 20 messages)
#define MAX_LINE    2   // 10  (0-10, 11 lines)
````

If using Ethernet, edit the sesTelnetServerConfig.h as appropriate:

```
// Define this to make all the strings live in Flash instead of RAM.
#define USE_FLASH
```

* If defined, the sesTelnetServer will be compiled to put all strings in flash
  storage. This is recommended and even required if doing the debug build,
  which has a ton of strings. However, if your application was simple, and you
  had enough free RAM, you could comment this out and the program would run
  faster without all the extra code to access flash storage.
  
```
// Define this to include printing basic Telnet protocol information. This
// will include a bunch of Flash strings.
#define TELNET_DEBUG // takes about 1176 bytes of Flash + 14 bytes of RAM.
```

* If defined, the sesTelnetServer will print out a ton of Telnet debug
  information on the protocol/commands being parsed. This is a fun way to see
  what all is going on with Telnet, but there is no reason you would want to
  enable this for production.

```  
// Define this to use multiserver support,but only if you have fixed your
// Ethernet library to allow it. See:
// http://subethasoftware.com/2013/04/09/arduino-ethernet-and-multiple-socket-
// server-connections/
//#define TELNET_MULTISERVER
```

* As of the time I created this code, there was a bug in the Arduino Ethernet
  library that was preventing it from handling multiple incoming socket
  connections. I hacked the library to allow this to work, and documented my
  changes at the above link. If your library is modified, this define will
  enable a "go away" connection. If someone is connected to the Arduino, and
  a second connection is attempted, that connection will connect and a
  "server is busy, please try later" message will be sent back. Without this,
  connections would just hang and timeout while the server is in use.

```  
// Configure telnet server MAC address and IP address.
byte mac[] FLASHMEM = { 0x2A, 0xA0, 0xD8, 0xFC, 0x8B, 0xEF };
byte ip[] FLASHMEM  = { 192, 168, 0, 200};
```

* Server MAC address and IP address. Standard Arduino ethernet library stuff.

```
// Define the ID string sent to the user upon initial connection.
#define TELNETID  "Sub-Etha Software's Arduino Telnet server."
#
// Define the AYT (Are You There) response string.
#define TELNETAYT "Yes. Why do you ask?"
```

* These two strings are used by the Telnet protocol. The first is an ID
  message sent to the client when they first connect, and the second is the
  response to the Telnet "ARE YOU THERE" command. A compliant Telnet server
  should be able to respond to AYT with a message, so I made the actual
  message user-definable since I could not find anything in the RFC that
  said what it was supposed to be.


RUNNING
=======
 
Good luck!

More to come...
