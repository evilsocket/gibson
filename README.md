Gibson [![Build Status](https://secure.travis-ci.org/evilsocket/gibson.png)](http://travis-ci.org/evilsocket/gibson)
===

A high performance tree-based cache server.  
**Still under heavy development, do not use in production environments**  
<http://www.emoticode.net/>  
<http://www.evilsocket.net/>

Features
---
* Very fast and with the lowest memory footprint possible
* Fast LZF object compression
* Builtin object Time-To-Live
* Cached object locking and unlocking
* Use prefix expressions with M* operators:  

For instance let's say you are using Gibson to cache your blog statistics, then you'll have something like:  

      INC /category/my-nice-post.html
or  

      INC /category/another-post-of-mine.html
  
To increment each url visit counter, then if you wanna do a mass increment on a given category:

      MINC /category/
      
Will increment both urls since both satisfy the '/category/' key prefix expression.


**More to come!**

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
