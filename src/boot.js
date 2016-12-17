//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo boot.js

(function boot0 (version) {

version= '0.18';

if ((function () { return this })().process) {
  process.versions.threads_a_gogo= version;
}

function boot (that,CHUNK,_on,_ntq) {

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
        q.once.forEach(function (v,i,o) { v.apply(that, argumentos) });
        delete q.once;
      }
      q.forEach(function (v,i,o) { v.apply(that, argumentos) });
    }
  }
  
  if (!(function () { return this })().process) {
    thread= that;
    that.on= on;
    that.once= once;
    that._on= _on= {};
    that._ntq= _ntq= [];
    that.nextTick= nextTick;
    that.removeAllListeners= removeAllListeners;
    that.version= version;
    return {dev:dispatchEvents, dnt:dispatchNextTicks};
  }
  else {
    that.on= on;
    that.once= once;
    that._on= _on= {};
    that.load= load;
    that.removeAllListeners= removeAllListeners;
    that.version= version;
    return dispatchEvents;
  }
  
}

return boot;
})()
