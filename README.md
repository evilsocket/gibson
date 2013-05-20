Gibson
---

A high performance pure cache server.  
( Still under heavy development, do not use in production environments )

Features:
---
* Very fast and with the lowest memory footprint possible
* Fast LZF object compression
* Builtin object Time-To-Live
* Cached object locking and unlocking operators
* More to come!

Compilation / Installation:
---
In order to compile gibson, you will need cmake and autotools installed, then:

* cd /path/to/gibson-source/
* cmake .
* make
* make install


License:
---

Released under the BSD license.  
Copyright (c) 2013, Simone Margaritelli <evilsocket at gmail dot com>  
All rights reserved.
