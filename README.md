Gibson [![Build Status](https://secure.travis-ci.org/evilsocket/gibson.png)](http://travis-ci.org/evilsocket/gibson)
===

* To report issues : <http://github.com/evilsocket/gibson/issues>
* Gibson website   : <http://gibson-db.in> 
* Documentation    : <http://gibson-db.in/documentation.php> 

**Table of Contents**

* [Introduction](#introduction)
* [Features](#features)
* [Compilation](#compilation)
* [Configuration](#configuration)
* [Using the console client](#using-the-console-client)
* [Command Reference](http://gibson-db.in/commands.php)
* [Clients](http://gibson-db.in/clients.php)
* [Protocol Specifications](http://gibson-db.in/protocol.php)
* [Data Model](#data-model)
* [License](#license)

* * *

## Introduction

Gibson is a high efficiency, [tree based](#datamodel) memory cache server.
Normal key-value stores ( memcache, Redis, etc ) uses a hash table as their main data structure, so every key is hashed with a specific algorithm and the resulting hash is used to identify the given value in memory. This approach, although very fast, doesn't allow the user to execute globbing expressions/selections on a given (multiple) keyset, thus resulting on a pure one-by-one access paradigm.
Gibson is different, it uses a special tree based structure allowing the user to perform operations on multiple key sets using a prefix expression achieving the same performance grades in the worst case, even better on an average case.
Unlike many other server applications, it's not multithreaded, but it uses multiplexing taking advantace of an event-driven network layer ( just like Node.js, or Nginx using libevent and so on ) which provides higher performances even on low cost hardware.

**You need Gibson if:**

* You need to cache result sets from heavy queries lowering your main database load and response times.
* You want to invalidate or rietrive multiple cached itemshierarchically with a single command given their common prefix, in lower than linear time.
* You need a cache backend which is fast, highly scalable and not redundant as the common key value store.

**You can't use Gibson to:**

* You can't use Gibson as a drop in replacement for your database, since it is NOT a database but a key value store.
* You need complex data structures such as lists, sets, etc to be cached as they are ( without serializing it ).
* You need to perform sorting on cached items.

## Features

* Very fast and with the lowest memory footprint possible
* Fast LZF object compression
* Builtin object Time-To-Live
* Cached object locking and unlocking
* Multiple key set operation with M* operators 

## Compilation

Gibson is still hosted as source code release on Github, in order to run it you will have to compile sources, therefore you will need **git**, **cmake**, **gcc** and **build-essential** packages installed on your computer.

You have two options to obtain the source code, one is cloning the github repository using git:

    git clone https://github.com/evilsocket/gibson.git

Or you can download the source code archive [from here](https://github.com/evilsocket/gibson/archive/unstable.zip).

Once you got the source code, all you have to do is compile Gibson, the process is pretty straightforward.

    $ cd gibson
    $ cmake . [compilation options]
    $ make
    # make install

Where **compilation options** might be:

    -DPREFIX=/your/custom/prefix

To use a different installation prefix rather than /usr

    -DWITH_DEBUG=1

To compile with debug symbols and without optimizations ( for devs ).

    -DWITH_JEMALLOC=1

To use the [jemalloc memory allocator](http://gibson-db.in/blog/gibson-is-now-optionally-jemalloc-powered.html) instead of the standard one.

## Configuration

After [compiling and installing](#compilation) Gibson source code release, you might want to edit the configuration file situated in
**/etc/gibson/gibson.conf**, even if [standard values](https://raw.github.com/evilsocket/gibson/master/gibson.conf) will fit the majority of situations.

Let's see each configuration directive purpose.

### logfile

The log file path, or /dev/stdout to log on the terminal output.

### loglevel

Integer number representing the verbosity of the log manager:

* 0 for **DEBUG** verbosity, every message even debug ones will be logged.
* 1 for **INFO** verbosity, only information messages, warning, errors and criticals will be logged.
* 2 for **WARNING** verbosity, only warnings, errors and criticals will be logged
* 3 for **ERROR** verbosity, only errors and criticals will be logged.
* 4 for **CRITICAL** verbosity, only segmentation faults will be logged.

### logflushrate

How often to flush logfile, where 1 stands for "flush the log file every new line".

### unix_socket

The UNIX socket path to use if Gibson will run in a local environment, use the directives **address** and **port** to create
a TCP server instead.

### address

Address to bind the TCP server to.

### port

TCP port to use for server listening.

### daemonize

If 1 the process server will be daemonized ( put on background ), otherwise will run synchronously with the caller process.

### pidfile

File to be used to save the current Gibson process id.

### max_memory

Maximum memory to be used by the Gibson server, when this value is reached, the server will try to deallocate old objects to free space ( see **gc_ratio** ) and, if 
there aren't freeable objects at the moment, will refuse to accept new objects with a **REPL_ERR_MEM** error reply.

### gc_ratio

If **max_memory** is reached, data that is not being accessed in this amount of time ( i.e. gc_ratio 1h = data that is not being accessed in the last hour ) get deleted to release memory for the server.

### max_item_ttl

Maximum time-to-live an object can have.

### max_idletime

Maximum time in seconds a client can be idle ( without read or write operations ), after this period the client connection will be closed.

### max_clients

Maximum number of clients Gibson can hadle concurrently.

### max_request_size

Maximum size of a client request.

### max_key_size

Maximum size of the key for a Gibson object.

### max_value_size

Maximum size of the value for a Gibson object.

### max_response_size

Maximum Gibson response size, used to limit I/O when a M* operator is used.

### compression

Objects above this size will be compressed in memory.

### cron_period

Number of milliseconds between each cron schedule, do not put a value higher than 1000.

### max_mem_cron

Check if max memory usage is reached every 'max_mem_cron' seconds.

### expired_cron

Check for expired items every 'expired_cron' seconds.

* * *

## Using the console client

Once your Gibson instance is up and running, you might want to download and install its [client library](https://github.com/evilsocket/libgibsonclient) which provides the default
console client too.

    $ git clone https://github.com/evilsocket/libgibsonclient.git
    $ cd libgibsonclient
    $ git submodule init
    $ git submodule update
    $ cmake .
    $ make
    # make install

The client command line arguments are pretty straightforward:
    # gibson-cli -h                                                                                                                             
    Gibson client utility.
    
    gibson-cli [-h|--help] [-a|--address ADDRESS] [-p|--port PORT] [-u|--unix UNIX_SOCKET_PATH]
    
      -h, --help            	  Print this help and exit.
      -a, --address ADDRESS   	  TCP address of Gibson instance.
      -p, --port PORT   		  TCP port of Gibson instance.
      -u, --unix UNIX_SOCKET_PATH Unix socket path of Gibson instance ( overrides TCP arguments ).

So if you want to connect to a TCP instance, you will type for instance:

    gibson-cli --address 127.0.0.1 --port 10128
    
Or, to connect to a Unix socket instance:

    gibson-cli --unix /var/run/gibson.sock

Once you are connected, you will see a prompt like this:

    type :quit or :q to quit.
    
    127.0.0.1:10128>

Now you can start using the client typing the command you want to execute.

For a complete command list ( and their syntax ), refer to the [protocol specs](http://gibson-db.in/protocol.php) and/or type the command string
to see its synax, for instance:

    127.0.0.1:10128> set
    ERROR: Invalid parameters, correct syntax is:
    	SET <ttl> <key> <value>

## Data Model

Most of the content of this document was taken from [this page](http://en.wikipedia.org/wiki/Trie) on Wikipedia.

Unlike many other key value stores, Gibson doesn't use a hash table as its main data structure, but a special type of tree called **Trie**

In computer science, a trie, also called digital tree or prefix tree, is an ordered tree data structure that is used to store a dynamic set or associative array where the keys are usually strings. Unlike a binary search tree, no node in the tree stores the key associated with that node; instead, its position in the tree defines the key with which it is associated. All the descendants of a node have a common prefix of the string associated with that node, and the root is associated with the empty string. Values are normally not associated with every node, only with leaves and some inner nodes that correspond to keys of interest. For the space-optimized presentation of prefix tree, see compact prefix tree.

The term trie comes from retrieval. This term was coined by Edward Fredkin, who pronounces it /ˈtriː/ "tree" as in the word retrieval.

In the example shown, keys are listed in the nodes and values below them. Each complete English word has an arbitrary integer value associated with it. A trie can be seen as a deterministic finite automaton, although the symbol on each edge is often implicit in the order of the branches.

It is not necessary for keys to be explicitly stored in nodes. (In the figure, words are shown only to illustrate how the trie works.)

Though tries are most commonly keyed by character strings, they need not be. The same algorithms can easily be adapted to serve similar functions of ordered lists of any construct, e.g., permutations on a list of digits or shapes. In particular, a bitwise trie is keyed on the individual bits making up a short, fixed size of bits such as an integer number or memory address.

![trie data model](http://upload.wikimedia.org/wikipedia/commons/thumb/b/be/Trie_example.svg/250px-Trie_example.svg.png)
A trie for keys "A", "to", "tea", "ted", "ten", "i", "in", and "inn".

### Advantages over a Hash Table

A trie can be used to replace a hash table, over which it has the following advantages:

* Looking up data in a trie is faster in the worst case, O(m) time (where m is the length of a search string), compared to an imperfect hash table. An imperfect hash table can have key collisions. A key collision is the hash function mapping of different keys to the same position in a hash table. The worst-case lookup speed in an imperfect hash table is O(N) time, but far more typically is O(1), with O(m) time spent evaluating the hash.
* There are no collisions of different keys in a trie.
* Buckets in a trie which are analogous to hash table buckets that store key collisions are necessary only if a single key is associated with more than one value.
* There is no need to provide a hash function or to change hash functions as more keys are added to a trie.
* A trie can provide an alphabetical ordering of the entries by key.

Tries do have some drawbacks as well:

* Tries can be slower in some cases than hash tables for looking up data, especially if the data is directly accessed on a hard disk drive or some other secondary storage device where the random-access time is high compared to main memory. ( **Not in the Gibson case** )
* Some keys, such as floating point numbers, can lead to long chains and prefixes that are not particularly meaningful. Nevertheless a bitwise trie can handle standard IEEE single and double format floating point numbers. ( **Not in the Gibson case** )
* Some tries can require more space than a hash table, as memory may be allocated for each character in the search string, rather than a single chunk of memory for the whole entry, as in most hash tables.

For more details refer to [this Wikipedia page](http://en.wikipedia.org/wiki/Trie).

## License

Released under the BSD license.  
Copyright &copy; 2013, Simone Margaritelli <evilsocket@gmail.com>  
All rights reserved.
