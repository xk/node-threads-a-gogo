//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo boot.js
  
(function boot (that,CHUNK,_on) {

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
  
  function dispatchEvents (event,args,q) {
    q= _on[event];
    if (q) {
      if (q.once) {
        q.once.forEach(function (v,i,o) { v.apply(that,args) });
        delete q.once;
      }
      q.forEach(function (v,i,o) { v.apply(that,args) });
    }
  }
  
  if (!(function () { return this })().process) {
    thread= that;
    that.on= on;
    that._on= _on= {};
    that._ntq= [];
    that.once= once;
    that.nextTick= nextTick;
    that.removeAllListeners= removeAllListeners;
    return {dev:dispatchEvents, dnt:dispatchNextTicks};
  }
  else {
    that.on= on;
    that._on= _on= {};
    that.once= once;
    that.load= load;
    that.removeAllListeners= removeAllListeners;
    return dispatchEvents;
  }
  
})
