(function (that) {
  'use strict';
  
  //2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo events.js
  
  that= this;

  function on (event,f,q) {
    (q= that._on[event]) ? q.push(f) : (that._on[event]= [f]);
    return that;
  }

  function once (event,f,q) {
    (q= that._on[event]) ? 0 : (q= that._on[event]= []);
    q.once ? q.once.push(f) : (q.once= [f]);
    return that;
  }
  
  function removeAllListeners (event) {
    event ? delete that._on[event] : (that._on= {});
    return that;
  }
  
  function dispatchEvents (event,args,q) {
    q= that._on[event];
    if (q) {
      if (q.once) {
        q.once.forEach(function (v,i,o) { v.apply(that,args) });
        delete q.once;
      }
      q.forEach(function (v,i,o) { v.apply(that,args) });
    }
  }
  
  that.on= on;
  that._on= {};
  that.once= once;
  that.removeAllListeners= removeAllListeners;
  
  return dispatchEvents;
})
