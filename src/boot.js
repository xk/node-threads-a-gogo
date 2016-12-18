//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo boot.js
//boot0 runs at module.init() which is at tagg= require('threads_a_gogo')
//boot1 runs twice at t=tagg.create(), first in node's main thread and again in the thread just .create()d

(function boot0 (version,global) {

version= '0.1.8';

global= (function () { return this })();
if (global.process) {
  process.versions.threads_a_gogo= version;
  if (!global.setImmediate) {
    global.setImmediate= function setImmediate (f) { process.nextTick(f) };
  }
}

function boot (that,CHUNK,_on,_ntq,global) {

  that= this;
  
  function nextTick (cb) {
    _ntq.push(cb);
    return that;
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

  function load (path, cb) {
    that.eval(require('fs').readFileSync(path, 'utf8'), cb);
    return that;
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
  
  function dispatchEvents (evento, argumentos) {
    var q= _on[evento];
    if (q) {
      if (q.once) {
        while (q.once.length) {
          q.once.shift().apply(that, argumentos);
        }
      }
      q.forEach(function (v,i,o) { v.apply(that, argumentos) });
    }
  }
  
  global= (function () { return this })();
  if (global.process) {
    that.on= on;
    that.once= once;
    that._on= _on= {};
    that.load= load;
    that.removeAllListeners= removeAllListeners;
    that.version= version;
    return dispatchEvents;
  }
  else {
    thread= that;
    that.on= on;
    that.once= once;
    that._on= _on= {};
    that._ntq= _ntq= [];
    that.nextTick= global.setImmediate= nextTick;
    that.removeAllListeners= removeAllListeners;
    that.version= version;
    return {dev:dispatchEvents, dnt:dispatchNextTicks};
  }
}

return boot;
})()
