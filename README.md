Gibson [![Build Status](https://secure.travis-ci.org/evilsocket/gibson.png)](http://travis-ci.org/evilsocket/gibson)
===

* To report issues : <http://github.com/evilsocket/gibson/issues>
* Gibson website   : <http://gibson-db.in> 
* Documentation    : <http://gibson-db.in/documentation/> 

## Introduction

Gibson is a high efficiency, tree based memory cache server.
Normal key-value stores ( memcache, Redis, etc ) uses a hash table as their main data structure, so every key is hashed with a specific algorithm and the resulting hash is used to identify the given value in memory. This approach, although very fast, doesn't allow the user to execute globbing expressions/selections on a given (multiple) keyset, thus resulting on a pure one-by-one access paradigm.
Gibson is different, it uses a special tree based structure allowing the user to perform operations on multiple key sets using a prefix expression achieving the same performance grades in the worst case, even better on an average case.
Unlike many other server applications, it's not multithreaded, but it uses multiplexing taking advantage of an event-driven network layer ( just like Node.js, or Nginx using libevent and so on ) which provides higher performances even on low cost hardware.

**You need Gibson if:**

* You need to cache result sets from heavy queries lowering your main database load and response times.
* You want to invalidate or rietrive multiple cached items hierarchically with a single command given their common prefix, in lower than linear time.
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

Documentation on <http://gibson-db.in/documentation/>

## License

Released under the BSD license.  
Copyright &copy; 2013, Simone Margaritelli <evilsocket@gmail.com>  
All rights reserved.

[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/evilsocket/gibson/trend.png)](https://bitdeli.com/free "Bitdeli Badge")
