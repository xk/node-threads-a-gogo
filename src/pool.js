
(function createPool (n) {
  'use strict';
  
  //2011-11, 2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo pool.js
  
  var o,tagg,jobsCtr;

  function load (path, cb) {
    o.pool.forEach(function (v,i,o) { v.load(path, cb) });
    return o;
  }


  function wrap (cb) {
    return function wrapcb (err, result) {
      jobsCtr-= 1;
      if (cb) cb.call(this, err, result);
    }
  }


  function evalAny (src, cb) {
/*
    TODO esto no es lo que debe ser, no es cuestión de elegir al buen tuntún
*/
    jobsCtr+= 1;
    o.pool[rnd(o.pool.length)].eval(src, wrap(cb));
    return o;
  }
  
  
  function evalAll (src, cb, i) {
    i= o.pool.length;
    while (i--) {
      jobsCtr+= 1;
      o.pool[i].eval(src, wrap(cb));
    };
    return o;
  }


  function emitAny (t, args) {
/*
    TODO esto no es lo que debe ser, no es cuestión de elegir al buen tuntún
*/
    args= Array.prototype.splice.call(arguments,0);
    t= o.pool[rnd(o.pool.length)];
    t.emit.apply(t, args);
    return o;
  }


  function emitAll (t, args, i) {
    args= Array.prototype.splice.call(arguments,0);
    i= o.pool.length;
    while (i--) {
      t= o.pool[i];
      t.emit.apply(t, args);
    }
    return o;
  }


  function on (event, cb) {
    o.pool.forEach(function (v,i,o) { v.on(event, cb) });
    return o;
  }


  function rnd (n) { return Math.floor(n * Math.random()) }
  
  
  function destroy (rudeza, sucb, ctr) {
    ctr= 0;
    function micb () {
      if (++ctr === o.pool.length) setImmediate(sucb);
    }
    if (typeof sucb !== 'function') sucb= 0;
    o.pool.forEach(function (v,i,o) { sucb ? v.destroy(rudeza, micb) : v.destroy(rudeza) });
    o.any.eval= o.any.emit= o.all.eval= o.all.emit= o.on= o.load= o.destroy= function err () {
      throw new Error('This thread pool has been destroyed');
    };
    o.pool= [];
  }


  n= Math.floor(n);
  if (!(n > 0)) {
    throw new Error('.createPool( numOfThreads ): numOfThreads must be a Number > 0');
  }
  
  tagg= this;
  jobsCtr= 0;
  o= {  
        load:load,
        on:on,
        any: { eval:evalAny, emit:emitAny },
        all: { eval:evalAll, emit:emitAll },
        totalThreads: function getTotalThreads () { return o.pool.length },
        idleThreads: function getIdleThreads () {
/*
    TODO esto no es lo que debe ser, espabilao!
*/
          return o.pool.length
        },
        pendingJobs: function getPendingJobs () { return jobsCtr },
        destroy:destroy,
        pool: [] };

  try {
    while (n--) o.pool[n]= tagg.create();
  }
  catch (e) {
    if (o.pool.length) {
      o.pool.length-= 1;
      while (o.pool.length) o.pool.pop().destroy(1);
    }
    throw e;
  }

  return o;
})
