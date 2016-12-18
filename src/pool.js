(function createPool (n,tagg) {
  'use strict';
  
  //2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo createPool.js
  /*
  
  ******          ALL THIS IS STILL BROKEN
  
  */
  tagg= this;
  
  n= Math.floor(n);
  if (!(n > 0)) {
    throw '.createPool( numOfThreads ): numOfThreads must be a Number > 0';
  }
  
  var kTypeRun= 1;
  var kTypeEmit= 2;
  var pool= [];
  var idleThreads= [];
  var q= { first:null, last:null, length:0 };
  var poolObject= {
    any: { eval:evalAny, emit:emitAny },
    all: { eval:evalAll, emit:emitAll },
    on: on,
    totalThreads: function getTotalThreads () { return pool.length },
    idleThreads: function getIdleThreads () { return idleThreads.length },
    pendingJobs: function getPendingJobs () { return q.length },
    destroy: destroy,
    load: poolLoad
  };
  
  try {
    while (n--) {
      pool[n]= idleThreads[n]= tagg.create();
    }
  }
  catch (e) {
    destroy(1);
    throw e;
  }



  function poolLoad (path, cb) {
    pool.forEach(function (v,i,o) { v.load(path,cb) });
    return poolObject;
  }


  function nextJob (t) {
    var job= qPull();
    if (job) {
      if (job.type === kTypeRun) {
        t.eval(job.srcTextOrEventType, function idleCB (e, d) {
          nextJob(t);
          var f= job.cbOrData;
          if (f) {
            job.cbOrData.call(t, e, d);
          }
        });
      }
      else if (job.type === kTypeEmit) {
        t.emit(job.srcTextOrEventType, job.cbOrData);
        nextJob(t);
      }
    }
    else {
      idleThreads.push(t);
    }
  }



  function qPush (srcTextOrEventType, cbOrData, type) {
    var job= { next:null, srcTextOrEventType:srcTextOrEventType, cbOrData:cbOrData, type:type };
    if (q.last) {
      q.last.next= job;
      q.last= job;
    }
    else {
      q.first= q.last= job;
    }
    q.length++;
  }



  function qPull () {
    var job= q.first;
    if (job) {
      if (q.last === job) {
        q.first= q.last= null;
      }
      else {
        q.first= job.next;
      }
      //job.next= null;
      q.length--;
    }
    return job;
  }



  function cbWrap (thread,cb) {
    
    return function wrappedCb () {
      
    };
  }



  function evalAny (src, cb) {
    if (idleThreads.length)
      idleThreads.pop().eval(src,cbWrap(cb));
    else
      jobsQueueAny.push({type:kTypeRun, src:src, cb:cb});
    return poolObject;
  }



  function evalAll (src, cb) {
    pool.forEach(function (v,i,o) { v.eval(src,cb) });
    return poolObject;
  }



  function emitAny (t,args) {
    args= Array.prototype.splice.call(arguments,0);
    if (idleThreads.length)
      (t= idleThreads.pop()).emit.apply(t,args);
    else
      jobsQueueAny.push({type:kTypeEmit, args:args});
    return poolObject;
  }



  function emitAll (args) {
    args= Array.prototype.splice.call(arguments,0);
    pool.forEach(function (v,i,o) { v.emit.apply(v,args) });
    return poolObject;
  }



  function on (event, cb) {
    pool.forEach(function (v,i,o) { v.on(event,cb) });
    return poolObject;
  }



  function destroy (rudely) {
    function err () {
      throw 'This thread pool has been destroyed';
    }
    
    function beNice () {
      if (q.length) {
        setTimeout(beNice, 666);
      }
      else {
        beRude();
      }
    }
    
    function beRude () {
      q.length= 0;
      q.first= null;
      pool.forEach(function (v,i,o) {
        v.destroy();
      });
      poolObject.eval= poolObject.totalThreads= poolObject.idleThreads= poolObject.pendingJobs= poolObject.destroy= err;
    }
    
    rudely ? beRude() : beNice();
  }



  return poolObject;
})
