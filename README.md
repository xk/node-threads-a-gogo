[![build status](https://travis-ci.org/xk/node-threads-a-gogo.svg?branch=SYNC)](https://travis-ci.org/xk/node-threads-a-gogo)
[![npm version](https://badge.fury.io/js/threads_a_gogo.svg)](https://www.npmjs.com/package/threads_a_gogo)

***

# TAGG: Threads à gogo for Node.js

Threads à gogo ([*](http://en.wikipedia.org/wiki/%C3%80_gogo)) is a native module for Node.js that provides an asynchronous, evented and/or continuation passing style API for moving blocking/longish CPU-bound tasks out of Node's event loop to JavaScript threads that run in parallel in the background and that use all the available CPU cores automatically; all from within **a single Node** process.

## Installing the module

With [npm](http://npmjs.org/):

    npm install threads_a_gogo

From source:

    git clone http://github.com/xk/node-threads-a-gogo.git
    cd node-threads-a-gogo
    # One of
    node-gyp rebuild
    node test/all.js
    # or
    npm install
    node test/all.js
    # or
    node-waf configure build install test
    # Depending of what wersion of node you've got.
    #
    # THREADS_A_GOGO CURRENTLY (v0.1.13) RUNS ON NODES v0.5.1 TO v6.9.2

Basic functionality test:

    cd node-threads-a-gogo
    node test/all.js
    # If all goes well should ouput something like this:
    0.OK.1.OK.2.OK.3.TAGG OBJECT OK
    4.OK.5.OK.6.OK.7.OK.8.OK.9.OK.10.OK.11.OK.12.THREAD OBJECT OK
    13.OK.WAITING FOR LOAD CB
    14.OK.15.LOAD CALLBACK OK
    16.OK.WAITING FOR EVAL CB
    17.OK.18.OK.19.EVAL CALLBACK OK
    20.OK.WAITING FOR EVENT LISTENER CB
    21.OK.22.OK.23.OK.24.OK.25.OK.26.EVENT LISTENER CB.OK
    27.OK.WAITING FOR DESTROY CB
    28.OK.29.DESTROY CB OK
    END
    THREADS_A_GOGO v0.1.13 BASIC FUNCTIONALITY TEST: OK, IT WORKS!

To include the module in your project:

    var tagg= require('threads_a_gogo');

**You need a node with a v8 >= 3.2.4 to run this module. Any node >= 0.5.1 comes with a v8 >= 3.2.4.**

The module **runs fine, though, in any node >= 0.1.13** as long as you build it with a v8 >= 3.2.4, [see here](https://nodejs.org/en/download/releases/). To do that you simply have to replace /node/deps/v8 with a newer version of v8 and recompile it (node). To get any version of node goto http://nodejs.org/dist/, and for v8 goto http://github.com/v8/v8, click on "branch", select the proper tag (>= 3.2.4), and download the .zip.

## Intro

After the initialization phase of a Node program, whose purpose is to setup listeners and callbacks to be executed in response to events, the next phase, the proper execution of the program, is orchestrated by the event loop whose duty is to [juggle events, listeners and callbacks quickly and without any hiccups nor interruptions that would ruin its performance](https://youtu.be/D0uA_NOb0PE?autoplay=1)

Both the event loop and said listeners and callbacks run sequentially in a single thread of execution, Node's main thread. If any of them ever blocks, nothing else will happen for the duration of the block: no more events will be handled, no more callbacks nor listeners nor timeouts nor nextTick()ed functions will have the chance to run and do their job, because they won't be called by the blocked event loop, and the program will turn sluggish at best, or appear to be frozen and dead at worst.

**A.-** Here's a program that makes Node's event loop spin freely and as fast as possible: it simply prints a dot to the console in each turn:

    cat examples/quickIntro_loop.js
    node examples/quickIntro_loop.js
    
``` javascript
(function spinForever () {
  process.stdout.write(".");
  setImmediate(spinForever);
})();
```

**B.-** Here's another program that adds to the one above a fibonacci(35) call in each turn, a CPU-bound task that takes quite a while to complete and that blocks the event loop making it spin slowly and clumsily. The point is simply to show that you can't put a job like that in the event loop because Node will stop performing properly when its event loop can't spin fast and freely due to a callback/listener/nextTick()ed function that's blocking.

    cat examples/quickIntro_blocking.js
    node examples/quickIntro_blocking.js

``` javascript
function fibo (n) {
  return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
}

(function fiboLoop () {
  process.stdout.write(fibo(35).toString());
  setImmediate(fiboLoop);
})();

(function spinForever () {
  process.stdout.write(".");
  setImmediate(spinForever);
})();
```

**C.-** The program below uses `threads_a_gogo` to run the fibonacci(35) calls in a background thread, so Node's event loop isn't blocked at all and can spin freely again at full speed:

    cat examples/quickIntro_oneThread.js
    node examples/quickIntro_oneThread.js
  
``` javascript
function fibo (n) {
  return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
}

function cb (err, data) {
  process.stdout.write(data);
  this.eval('fibo(35)', cb);
}

var thread= require('threads_a_gogo').create();

thread.eval(fibo).eval('fibo(35)', cb);

(function spinForever () {
  process.stdout.write(".");
  setImmediate(spinForever);
})();
```

**D.-** This example is almost identical to the one above, only that it creates 5 threads instead of one, each running a fibonacci(35) in parallel and in parallel too with Node's event loop that keeps spinning happily at full speed in its own thread:

    cat examples/quickIntro_fiveThreads.js
    node examples/quickIntro_fiveThreads.js
  
``` javascript
function fibo (n) {
  return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
}

function cb (err, data) {
  process.stdout.write(" ["+ this.id+ "]"+ data);
  this.eval('fibo(35)', cb);
}

var tagg= require('threads_a_gogo');

tagg.create().eval(fibo).eval('fibo(35)', cb);
tagg.create().eval(fibo).eval('fibo(35)', cb);
tagg.create().eval(fibo).eval('fibo(35)', cb);
tagg.create().eval(fibo).eval('fibo(35)', cb);
tagg.create().eval(fibo).eval('fibo(35)', cb);

(function spinForever () {
  process.stdout.write(".");
  setImmediate(spinForever);
})();
```

**E.-** The next one asks `threads_a_gogo` to create a pool of 10 background threads, instead of creating them manually one by one:

    cat examples/quickIntro_multiThread.js
    node examples/quickIntro_multiThread.js

``` javascript
function fibo (n) {
  return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
}

var numThreads= 10;
var threadPool= require('threads_a_gogo').createPool(numThreads).all.eval(fibo);

threadPool.all.eval('fibo(35)', function cb (err, data) {
  process.stdout.write(" ["+ this.id+ "]"+ data);
  this.eval('fibo(35)', cb);
});

(function spinForever () {
  process.stdout.write(".");
  setImmediate(spinForever);
})();
```

**F.-** This is a demo of the `threads_a_gogo` eventEmitter API, using one thread:

    cat examples/quickIntro_oneThreadEvented.js
    node examples/quickIntro_oneThreadEvented.js

``` javascript
var thread= require('threads_a_gogo').create();
thread.load(__dirname + '/quickIntro_evented_childThreadCode.js');

/*
  This is the code that's .load()ed into the child/background thread:
  
  function fibo (n) {
    return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
  }

  thread.on('giveMeTheFibo', function onGiveMeTheFibo (data) {
    this.emit('theFiboIs', fibo(+data)); //Emits 'theFiboIs' in the parent/main thread.
  });
  
*/

//Emit 'giveMeTheFibo' in the child/background thread.
thread.emit('giveMeTheFibo', 35);

//Listener for the 'theFiboIs' events emitted by the child/background thread.
thread.on('theFiboIs', function cb (data) {
  process.stdout.write(data);
  this.emit('giveMeTheFibo', 35);
});

(function spinForever () {
  process.stdout.write(".");
  setImmediate(spinForever);
})();
```

**G.-** This is a demo of the `threads_a_gogo` eventEmitter API, using a pool of threads:

    cat examples/quickIntro_multiThreadEvented.js
    node examples/quickIntro_multiThreadEvented.js

``` javascript
var numThreads= 10;
var threadPool= require('threads_a_gogo').createPool(numThreads);
threadPool.load(__dirname + '/quickIntro_evented_childThreadCode.js');

/*
  This is the code that's .load()ed into the child/background threads:
  
  function fibo (n) {
    return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
  }

  thread.on('giveMeTheFibo', function onGiveMeTheFibo (data) {
    this.emit('theFiboIs', fibo(+data)); //Emits 'theFiboIs' in the parent/main thread.
  });
  
*/

//Emit 'giveMeTheFibo' in all the child/background threads.
threadPool.all.emit('giveMeTheFibo', 35);

//Listener for the 'theFiboIs' events emitted by the child/background threads.
threadPool.on('theFiboIs', function cb (data) {
  process.stdout.write(" ["+ this.id+ "]"+ data);
  this.emit('giveMeTheFibo', 35);
});

(function spinForever () {
  process.stdout.write(".");
  setImmediate(spinForever);
})();
```

## More examples

The `examples` directory contains a few more examples:

* [ex01_basic](https://github.com/xk/node-threads-a-gogo/blob/master/examples/ex01_basic.md): Running a simple function in a thread.
* [ex02_events](https://github.com/xk/node-threads-a-gogo/blob/master/examples/ex02_events.md): Sending events from a worker thread.
* [ex03_ping_pong](https://github.com/xk/node-threads-a-gogo/blob/master/examples/ex03_ping_pong.md): Sending events both ways between the main thread and a worker thread.
* [ex04_main](https://github.com/xk/node-threads-a-gogo/blob/master/examples/ex04_main.md): Loading the worker code from a file.
* [ex05_pool](https://github.com/xk/node-threads-a-gogo/blob/master/examples/ex05_pool.md): Using the thread pool.
* [ex06_jason](https://github.com/xk/node-threads-a-gogo/blob/master/examples/ex06_jason.md): Passing complex objects to threads.

## Module API

## TAGG Object API
``` javascript
tagg= require('threads_a_gogo') -> tagg object

{ create: [Function],
  createPool: [Function: createPool],
  version: '0.1.13' }
  
```
### .create()
`tagg.create( /* no arguments */ )` -> thread object

### .createPool( numThreads )
`tagg.createPool( numberOfThreads )` -> threadPool object

### .version
`tagg.version` -> A string with the threads_a_gogo version number.

***

## Thread Object API (this is the thread object you get in node's main thread)
``` javascript
thread= tagg.create() -> thread object

{ load: [Function: load],
  eval: [Function: eval],
  emit: [Function: emit],
  destroy: [Function: destroy],
  id: 0,
  version: '0.1.13',
  on: [Function: on],
  once: [Function: once],
  _on: {},
  removeAllListeners: [Function: removeAllListeners] }
  
```
### .id
`thread.id` -> A sequential thread serial number.

### .version
`thread.version` -> A string with the threads_a_gogo version number.

### .load( path [, cb] )
`thread.load( path [, cb] )` -> thread

Loads the file at `path` and `thread.eval(fileContents, cb)`.

NOTE that most methods return the `thread` object so that calls can be chained like this: `thread.load("boot.js").eval("boot()").emit("go").on("event", cb)` ...

### .eval( program [, cb])
`thread.eval( program [, cb])` -> thread

Converts `program.toString()` and eval()s (which is the equivalent of loading a script in a browser) it in the thread's global context, and (if provided) returns to a callback the completion value: `cb(err, completionValue)`. Some examples you can try in node's console, but first please declare a `function cb (a,b) { console.log(a,b) }`, that will be handy, then type `thread.eval("Math.random()", cb)` the thread will run that (`Math.random()`) and the `cb(err,result)` will get (and display) a `null (random number)`. A `thread.eval('function hello () { puts("Hello!"); return "world!" }', cb)` will eval that in the global context of the thread and thus create a global function hello in the thread's js global context and your `cb(err,result)` will get `null undefined` in the result, `null` because there were no errors and `undefined` because the completion value of a function declaration is `undefined`. On the plus side, now you have injected for the first time in your life some js code of yours in a TAGG's thread and you can tell it to run that, do a `thread.eval('hello()',cb)` and the thread will print `Hello!` in the console and the `cb(err,result)` will receive `world!` into result. How cool is that?

### .on( eventType, listener )
`thread.on( eventType, listener )` -> thread

Registers the listener `listener(data [, data2 ... ])` for any events of `eventType` that the thread `thread` may emit. For example, declare a `function cb (a,b,c) { console.log(a,b,c) }` and then do `thread.on("event", cb)`. Now whenever the thread emits an event of the type `event` (which by the way can be any arbitrary name/string you choose), `cb` will be triggered. Let's do that with a `thread.eval('i=5; while (i--) thread.emit("event", "How", "cool", "is that?")')` and the console will display `How cool is that?` five times, huh, unbelievable.

### .once( eventType, listener )
`thread.once( eventType, listener )` -> thread

Like `thread.on()`, but the listener will only be triggered once.

### .removeAllListeners( [eventType] )
`thread.removeAllListeners( [eventType] )` -> thread

Deletes all listeners for all eventTypes unless `eventType` is provided, in which case it deletes the listeners only of the event type `eventType`.

### .emit( eventType, eventData [, eventData ... ] )
`thread.emit( eventType, eventData [, eventData ... ] )` -> thread

Emit an event of `eventType` with `eventData` in the thread `thread`. All its arguments are .toString()ed.

### .destroy( [ rudely ] )
`thread.destroy( [0 (nicely) | 1 (rudely)] [, cb])` -> undefined

Destroys the thread. If the first parameter is not provided or falsy or 0 (the default) the thread will keep running until both its nextTick/setImmediate queue and its pending `.eval()` jobs queue are empty. If it's truthy or 1 (rudely) the thread's event loop will exit as soon as possible, regardless. If a callback cb (optional) is provided, it will be called when the thread has been killed and completely destroyed, the cb will receive no arguments and the receiver (its 'this') points to the global object. If the thread is stuck in a while (1) ; or similar it won't end and currently tagg has no way around that. At least not yet. Pull requests are very much welcomed, just so you know.

### _on
Ignore, don't touch that. The `_on` object holds the event listeners.

***

## Thread object API (this is the thread object that exists not in node but as a global in every thread)

Inside every thread .create()d by threads_a_gogo, there's a global `thread` object with these properties:
``` javascript
thread (a global) ->

{ id: 0,
  version: '0.1.13',
  on: [Function: on],
  once: [Function: once],
  emit: [Function: emit],
  removeAllListeners: [Function: removeAllListeners]
  nextTick: [Function: nextTick],
  _on: {},
  _ntq: [] }

```
### .id
`thread.id` -> the serial number of this thread.

### .version
`thread.version` -> A string with the threads_a_gogo version number.

### .on( eventType, listener )
`thread.on( eventType, listener )` -> thread

Just like `thread.on()` above. But in this case it will receive events emitted by node's main thread, because, remeber, all this exists in the thread's own js context which is totally independent of node's js context.

### .once( eventType, listener )
`thread.once( eventType, listener )` -> thread

Just like `thread.once()` above.

### .emit( eventType, eventData [, eventData ... ] )
`thread.emit( eventType, eventData [, eventData ... ] )` -> thread

Just like `thread.emit()` above. What this emits will trigger a listener (if set) in node's main thread.

### .removeAllListeners( [eventType] )
`thread.removeAllListeners( [eventType] )` -> thread

Just like `thread.removeAllListeners()` above.

### .nextTick( function )
`thread.nextTick( function )` -> undefined

Like `setImmediate()`, but twice as fast. Every thread runs its own event loop. First it looks for any .eval()s that node may have sent, if there are any it runs the first one, after that it looks in the nextTick/setImmediate queue, if there are any functions there to run, it runs them all (but only in chunks of up to 8192 so that nextTick/setImmediate can never totally block a thread), and then looks again to see if there are any more .eval() events, if there are runs the first one, and repeats this loop forever until there are no more .eval() events nor nextTick/setImmediate functions at which point the thread goes to sleep until any further .eval()s or .emit()s sent from node awake it.

### _on
Ignore, don't touch that. The `_on` object holds the event listeners.

### _ntq
Ignore, don't touch that. The `_ntq` array holds the nextTick()ed and/or setImmediate()d funcitons.

***

## Globals in the threads' js contexts

Inside every thread .create()d by threads_a_gogo, on top of the usual javascript ones there's these other globals:

### puts(arg1 [, arg2 ...])
`puts(arg1 [, arg2 ...])` -> undefined

.toString()s and fprintf(stdout)s and fflush(stdout)es its arguments to (you guessed it) stdout.

### setImmediate(function)
`setImmediate( function )` -> undefined

Just an alias for `thread.nextTick(function)`.

### thread
The thread object described above is also a global.

***

## Thread pool API
``` javascript
pool= tagg.createPool( numbreOfThreads ) ->

{ load: [Function: load],
  on: [Function: on],
  any: 
   { eval: [Function: evalAny],
     emit: [Function: emitAny] },
  all: 
   { eval: [Function: evalAll],
     emit: [Function: emitAll] },
  totalThreads: [Function: getTotalThreads],
  idleThreads: [Function: getIdleThreads],
  pendingJobs: [Function: getPendingJobs],
  destroy: [Function: destroy],
  pool: 
   [ { load: [Function: load],
       eval: [Function: eval],
       emit: [Function: emit],
       destroy: [Function: destroy],
       id: 0,
       version: '0.1.13',
       on: [Function: on],
       once: [Function: once],
       _on: {},
       removeAllListeners: [Function: removeAllListeners] } ] }

```
### .load( path [, cb] )
`pool.load( path [, cb] )` -> pool

`thread.load( path [, cb] )` in every one of the pool's threads. The cb will be called as many times as threads there are.

### .on( eventType, listener )
`pool.on( eventType, listener )` -> pool

Registers a listener for events of eventType. Any events of eventType emitted by any thread of the pool will trigger that cb/listener. For example, if you set a `pool.on("event", cb)` and any thread does a `thread.emit("event", "Hi", "there")`, a `cb(a,b)` will receive "Hi" in a and "There" in b.

### .any.eval( program, cb )
`pool.any.eval( program, cb )` -> pool

Like `thread.eval()`, but in any of the pool's threads.

### .any.emit( eventType, eventData [, eventData ... ] )
`pool.any.emit( eventType, eventData [, eventData ... ] )` -> pool

Like `thread.emit()` but in any of the pool's threads.

### .all.eval( program, cb )
`pool.all.eval( program, cb )` -> pool

Like `thread.eval()`, but in all the pool's threads.

### .all.emit( eventType, eventData [, eventData ... ] )
`pool.all.emit( eventType, eventData [, eventData ... ] )` -> pool

Like `thread.emit()` but in all the pool's threads.

### .totalThreads()
`pool.totalThreads()` -> returns the number of threads in this pool: as supplied in `.createPool( number )`. It's also the same as `pool.pool.length`.

### .idleThreads()
`pool.idleThreads()` -> returns the number of threads in this pool that are currently idle. Does not work very well currently. Better don't use it.

### .pendingJobs()
`threadPool.pendingJobs()` -> returns the number of jobs pending. Does not work very well currently. Better don't use it.

### .destroy( [ rudely ] )
`pool.destroy( [ rudely ] )` -> undefined

If rudely is 0 or falsy the thread will exit when there aren't any more events in its events queue. If rudely it will quit regardless of that. If stuck in a while (1) ; or similar it won't and currently tagg has no way around that. At least not yet. Pull requests are very much welcomed, just so you know.

### .pool
`pool.pool` is an array that contains all the threads in the thread pool for your tinkering pleasure.

## Rationale

[Node.js](http://nodejs.org) is the most [awesome, cute and super-sexy](http://javascriptology.com/threads_a_gogo/sexy.jpg) piece of free, open source software.

Its event loop can spin as fast and smooth as a turbo, and roughly speaking, **the faster it spins, the more power it delivers**. That's why [@ryah](http://twitter.com/ryah) took great care to ensure that no -possibly slow- I/O operations could ever block it: a pool of background threads (thanks to [Marc Lehmann's libeio library](http://software.schmorp.de/pkg/libeio.html)) handle any blocking I/O calls in the background, in parallel.

In Node it's verboten to write a server like this:

``` javascript
http.createServer(function (req,res) {
  res.end( fs.readFileSync(path) );
}).listen(port);
```
Because synchronous I/O calls **block the turbo**, and without proper boost, Node.js begins to stutter and behaves clumsily. To avoid it there's the asynchronous version of `.readFile()`, in continuation passing style, that takes a callback:

``` javascript
fs.readfile(path, function cb (err, data) { /* ... */ });
```

It's cool, we love it (*), and there's hundreds of ad hoc built-in functions like this in Node to help us deal with almost any variety of possibly slow, blocking I/O.

### But what's with longish, CPU-bound tasks?

How do you avoid blocking the event loop, when the task at hand isn't I/O bound, and lasts more than a few fractions of a millisecond?

``` javascript
http.createServer(function cb (req,res) {
  res.end( fibonacci(40) );
}).listen(port);
```

You simply can't, because there's no way... well, there wasn't before `threads_a_gogo`.

### What is Threads A GoGo for Node.js

`threads_a_gogo` provides the asynchronous API for CPU-bound tasks that's missing in Node.js. Both in continuation passing style (callbacks), and in event emitter style (event listeners).

The same API Node uses to delegate a longish I/O task to a background (libeio) thread:

`asyncIOTask(what, cb);`

`threads_a_gogo` uses to delegate a longish CPU task to a background (JavaScript) thread:

`thread.eval(program, cb);`

So with `threads_a_gogo` you can write:

``` javascript
http.createServer(function (req,res) {
  thread.eval('fibonacci(40)', function cb (err, data) {
    res.end(data);
  });
}).listen(port);
```

And it won't block the event loop because the `fibonacci(40)` will run in parallel in a separate background thread.


### Why Threads

Threads (kernel threads) are very interesting creatures. They provide:

1.- Parallelism: All the threads run in parallel. On a single core processor, the CPU is switched rapidly back and forth among the threads providing the illusion that the threads are running in parallel, albeit on a slower CPU than the real one. With 10 compute-bound threads in a process, the threads would appear to be running in parallel, each one on a CPU with 1/10th the speed of the real CPU. On a multi-core processor, threads are truly running in parallel, and get time-sliced when the number of threads exceed the number of cores. So with 12 compute bound threads on a quad-core processor each thread will appear to run at 1/3rd of the nominal core speed.

2.- Fairness: No thread is more important than another, cores and CPU slices are fairly distributed among threads by the OS scheduler.

3.- Threads fully exploit all the available CPU resources in your system. On a loaded system running many tasks in many threads, the more cores there are, the faster the threads will complete. Automatically.

4.- The threads of a process share exactly the same address space, that of the process they belong to. Every thread can access every memory address within the process' address space. This is a very appropriate setup when the threads are actually part of the same job and are actively and closely cooperating with each other. Passing a reference to a chunk of data via a pointer is many orders of magnitude faster than transferring a copy of the data via IPC.


### Why not multiple processes.

The "can't block the event loop" problem is inherent to Node's evented model. No matter how many Node processes you have running as a [Node-cluster](http://blog.nodejs.org/2011/10/04/an-easy-way-to-build-scalable-network-programs/), it won't solve its issues with CPU-bound tasks.

Launch a cluster of N Nodes running the example B (`quickIntro_blocking.js`) above, and all you'll get is N -instead of one- Nodes with their event loops blocked and showing a sluggish performance.
