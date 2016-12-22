//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo boot.js
//boot0 runs at module.init() which is at tagg= require('threads_a_gogo')
//boot1 runs twice at t=tagg.create(), first in node's main thread and
//again in the thread just .create()d

(function boot0 (global) {

  global= (function () { return this })();

  if (global.process) {
    if (!global.setImmediate) {
      global.setImmediate= function setImmediate (f) {
        process.nextTick(f);
      };
    }
  }

  function boot (that,global,CHUNK,_on,_ntq) {

    that= this;
    global= (function () { return this })();

    function nextTick (cb) {
      _ntq.push(cb);
    }

    CHUNK= 8192;
    function dispatchNextTicks (len,i) {
      if (_ntq.length) {
        len= _ntq.length > CHUNK ? CHUNK : _ntq.length;
        i= 0;
        try {
          do { _ntq[i++]() } while (i<len);
          that._ntq= _ntq= _ntq.splice(i);
        }
        catch (e) {
          that._ntq= _ntq= _ntq.splice(i);
          throw e;
        }
      }
      return _ntq.length;
    }

    function on (event,f,q) {
      (q= _on[event]) ? q.push(f) : (_on[event]= [f]);
      return that;
    }

    function once (event,f,q) {
      (q= _on[event]) ? 0 : (q= _on[event]= []);
      q.once ? q.once.push(f) : (q.once= [f]);
      return that;
    }

    function removeAllListeners (event) {
      event ? delete _on[event] : (that._on= _on= {});
      return that;
    }

    function dispatchEvents (event,argumentos,i) {
      var q= _on[event];
      if (q) {
        if (q.once) {
          while (q.once.length) {
            q.once.shift().apply(that, argumentos);
          }
        }
        for ( i=0 ; i<q.length ; i++ ) {
          q[i].apply(that, argumentos);
        }
      }
    }
  

    if (global.process) {
      that.on= on;
      that.once= once;
      that._on= _on= {};
      that.removeAllListeners= removeAllListeners;
      return dispatchEvents;
    }
    else {
      thread= that;that.on= on;
      that.once= once;
      that._on= _on= {};
      that._ntq= _ntq= [];
      that.removeAllListeners= removeAllListeners;
      that.nextTick= global.setImmediate= nextTick;
      return {dev:dispatchEvents, dnt:dispatchNextTicks};
    }
  }

  return boot;

})()
