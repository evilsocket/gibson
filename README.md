Gibson [![Build Status](https://secure.travis-ci.org/evilsocket/gibson.png)](http://travis-ci.org/evilsocket/gibson)
===

Gibson is a high efficiency, tree based memory cache server. It is not meant to replace a database, since it was written to be a key-value store to be used as a cache server, but it's not the usual cache server.
Normal key-value stores ( memcache, redis, etc ) uses a hash table as their main data structure, so every key is hashed with a specific algorithm and the resulting hash is used to identify the given value in memory. This approach, although very fast, doesn't allow the user to execute globbing expressions/selections with on a given keyset, thus resulting on a pure one-by-one access paradigm.
Gibson is different, it uses a tree based structure allowing the user to perform operations on multiple key sets using a prefix expression achieving the same performance grades

<http://gibson-db.in/>  

Features
---
* Very fast and with the lowest memory footprint possible
* Fast LZF object compression
* Builtin object Time-To-Live
* Cached object locking and unlocking
* Multiple key set operation with M* operators 


Compilation / Installation
---
In order to compile gibson, you will need cmake and autotools installed, then:

    $ cd /path/to/gibson-source/
    $ cmake .
    $ make
    # make install


Usage
---

    gibson [options]
    Options:
      -h, --help                        Print this help menu and exit.
      -c, --config VALUE                Set configuration file to load.
    
      --logfile VALUE                   The log file path, or /dev/stdout to log on the terminal output.
      --loglevel VALUE                  Integer number representing the verbosity of the log manager.
      --logflushrate VALUE              How often to flush logfile, where 1 stands for 'flush the log file every new line'.
      --unix_socket VALUE               The UNIX socket path to use if Gibson will run in a local environment, use the directives address and port to create a TCP server instead.
      --address VALUE                   Address to bind the TCP server to.
      --port VALUE                      TCP port to use for server listening.
      --max_idletime VALUE              Maximum time in seconds a client can be idle ( without read or write operations ), after this period the client connection will be closed.
      --max_clients VALUE               Maximum number of clients Gibson can hadle concurrently.
      --max_request_size VALUE          Maximum size of a client request.
      --max_item_ttl VALUE              Maximum time-to-live an object can have.
      --max_memory VALUE                Maximum memory to be used by the Gibson server, when this value is reached, the server will try to deallocate old objects to free space ( see gc_ratio ) and, if there aren't freeable objects at the moment, will refuse to accept new objects with a REPL_ERR_MEM error reply.
      --max_key_size VALUE              Maximum size of the key for a Gibson object.
      --max_value_size VALUE            Maximum size of the value for a Gibson object.
      --max_response_size VALUE         Maximum Gibson response size, used to limit I/O when a M* operator is used.
      --compression VALUE               Objects above this size will be compressed in memory.
      --daemonize VALUE                 If 1 the process server will be daemonized ( put on background ), otherwise will run synchronously with the caller process.
      --cron_period VALUE               Number of milliseconds between each cron schedule, do not put a value higher than 1000.
      --pidfile VALUE                   File to be used to save the current Gibson process id.
      --gc_ratio VALUE                  If max_memory is reached, data that is not being accessed in this amount of time ( i.e. gc_ratio 1h = data that is not being accessed in the last hour ) get deleted to release memory for the server.

    To report issues : <http://github.com/evilsocket/gibson/issues>
    Gibson website   : <http://gibson-db.in>
    Documentation    : <http://gibson-db.in/documentation.php>

License
---

Released under the BSD license.  
Copyright &copy; 2013, Simone Margaritelli <evilsocket@gmail.com>  
All rights reserved.
