(function (that,CHUNK) {
  'use strict';
  
  //2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo nextTick.js
  
  that= this;
  
  function nextTick (cb) {
    that._ntq.push(cb);
    return that;
  }

  CHUNK= 8192;
  function dispatchNextTicks (len,i) {
    if (that._ntq.length) {
      len= that._ntq.length > CHUNK ? CHUNK : that._ntq.length;
      i= 0;
      try {
        do { that._ntq[i++]() } while (i<len);
        that._ntq= that._ntq.splice(i);
      }
      catch (e) {
        that._ntq= that._ntq.splice(i);
        throw e;
      }
    }
    return that._ntq.length;
  }

  that._ntq= [];
  that.nextTick= nextTick;
  return dispatchNextTicks;
})
