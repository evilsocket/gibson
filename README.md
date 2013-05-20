Gibson
---

A high performance pure cache server.  
**Still under heavy development, do not use in production environments**  
<http://www.emoticode.net/>  
<http://www.evilsocket.net/>

Features
---
* Very fast and with the lowest memory footprint possible
* Fast LZF object compression
* Builtin object Time-To-Live
* Cached object locking and unlocking operators
* More to come!

Compilation / Installation
---
In order to compile gibson, you will need cmake and autotools installed, then:

    $ cd /path/to/gibson-source/
    $ cmake .
    $ make
    # make install


Usage
---

    gibson [-h|--help] [-c|--config FILE]
																																																												 
        -h, --help          print this help and exit
        -c, --config FILE   set configuration file to load

License
---

Released under the BSD license.  
Copyright &copy; 2013, Simone Margaritelli <evilsocket@gmail.com>  
All rights reserved.
