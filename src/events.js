(function (thread) {
  'use strict';
  
  //2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo_events.js
  
  thread= this;

  function on (event,f,q) {
    (q= thread._on[event]) ? q.push(f) : (thread._on[event]= [f]);
    return thread;
  }

  function once (event,f,q) {
    (q= thread._on[event]) ? 0 : (q= thread._on[event]= []);
    q.once ? q.once.push(f) : (q.once= [f]);
    return thread;
  }
  
  function removeAllListeners (event) {
    event ? delete thread._on[event] : (thread._on= {});
    return thread;
  }
  
  function dispatchEvents (event,args,q) {
    q= thread._on[event];
    if (q) {
      if (q.once) {
        q.once.forEach(function (v,i,o) { v.apply(thread,args) });
        delete q.once;
      }
      q.forEach(function (v,i,o) { v.apply(thread,args) });
      q= q.once;
    }
  }
  
  thread.on= on;
  thread._on= {};
  thread.once= once;
  thread.removeAllListeners= removeAllListeners;
  
  return dispatchEvents;
})
