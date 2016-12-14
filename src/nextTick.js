(function (thread,CHUNK) {
  'use strict';
  
  //2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo_thread_nextTicks.js
  
  thread= this;
  
  function nextTick (cb) {
    thread._ntq.push(cb);
    return this;
  }

  CHUNK= 8192;
  function dispatchNextTicks (len,i) {
    if (thread._ntq.length) {
      len= thread._ntq.length > CHUNK ? CHUNK : thread._ntq.length;
      i= 0;
      try {
        do { thread._ntq[i++]() } while (i<len);
        thread._ntq= thread._ntq.splice(i);
      }
      catch (e) {
        thread._ntq= thread._ntq.splice(i);
        throw e;
      }
    }
    return thread._ntq.length;
  }

  thread._ntq= [];
  thread.nextTick= nextTick;
  return dispatchNextTicks;
})
