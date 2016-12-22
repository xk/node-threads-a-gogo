
(function createPool (n) {
  'use strict';
  
  //2011-11, 2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo pool.js
  
  var o,pool,tagg,jobsCtr;

  function load (path, cb) {
    pool.forEach(function (v,i,o) { v.load(path, cb) });
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
    pool[rnd(pool.length)].eval(src, wrap(cb));
    return o;
  }
  
  
  function evalAll (src, cb, i) {
    i= pool.length;
    while (i--) {
      jobsCtr+= 1;
      pool[i].eval(src, wrap(cb));
    };
    return o;
  }


  function emitAny (t, args) {
/*
    TODO esto no es lo que debe ser, no es cuestión de elegir al buen tuntún
*/
    args= Array.prototype.splice.call(arguments,0);
    t= pool[rnd(pool.length)];
    t.emit.apply(t, args);
    return o;
  }


  function emitAll (t, args, i) {
    args= Array.prototype.splice.call(arguments,0);
    i= pool.length;
    while (i--) {
      t= pool[i];
      t.emit.apply(t, args);
    }
    return o;
  }


  function on (event, cb) {
    pool.forEach(function (v,i,o) { v.on(event, cb) });
    return o;
  }


  function rnd (n) { return Math.floor(n * Math.random()) }
  
  
  function destroy (rudeza, sucb, ctr) {
    ctr= 0;
    function micb () {
      if (++ctr === pool.length) setImmediate(sucb);
    }
    if (typeof sucb !== 'function') sucb= 0;
    pool.forEach(function (v,i,o) { sucb ? v.destroy(rudeza, micb) : v.destroy(rudeza) });
    o.any.eval= o.any.emit= o.all.eval= o.all.emit= o.on= o.load= o.destroy= function err () {
      throw new Error('This thread pool has been destroyed');
    };
    pool= [];
  }


  n= Math.floor(n);
  if (!(n > 0)) {
    throw new Error('.createPool( numOfThreads ): numOfThreads must be a Number > 0');
  }
  
  tagg= this;
  jobsCtr= 0;
  pool= [];
  o= {  any: { eval:evalAny, emit:emitAny },
        all: { eval:evalAll, emit:emitAll },
        on:on,
        totalThreads: function getTotalThreads () { return pool.length },
        idleThreads: function getIdleThreads () {
/*
    TODO esto no es lo que debe ser, espabilao!
*/
          return pool.length
        },
        pendingJobs: function getPendingJobs () { return jobsCtr },
        load:load,
        destroy:destroy };

  try {
    while (n--) pool[n]= tagg.create();
  }
  catch (e) {
    if (pool.length) {
      pool.length-= 1;
      while (pool.length) pool.pop().destroy(1);
    }
    throw e;
  }

  return o;
})
