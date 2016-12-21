(function createPool (n,o,pool,tagg) {
  'use strict';
  
  //2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo createPool.js
  /*
  
  ******          ALL THIS IS STILL BROKEN
  
  */
  

  function load (path, cb) {
    pool.forEach(function (v,i,o) { v.load(path, cb) });
    return o;
  }


  function evalAny (src, cb) {
    pool[rnd(pool.length)].eval(src, cb);
    return o;
  }


  function evalAll (src, cb) {
    pool.forEach(function (v,i,o) {
      v.eval(src, cb);
    });
    return o;
  }


  function emitAny (t, args) {
    args= Array.prototype.splice.call(arguments,0);
    t= pool[rnd(pool.length)];
    t.emit.apply(t, args);
    return o;
  }


  function emitAll (t, args) {
    args= Array.prototype.splice.call(arguments,0);
    pool.forEach(function (v,i,o) { v.emit.apply(v, args) });
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
  
  pool= [];
  o= {  any: { eval:evalAny, emit:emitAny },
        all: { eval:evalAll, emit:emitAll },
        on:on,
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
