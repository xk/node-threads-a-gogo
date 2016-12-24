//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo test/00_events_storm.js

//Esto emite unos cuantos miles de eventos de distintos tipos y con distintos datos y longitudes
//Y se asegura de que los listeners reciben correctamente lo enviado.
//Es importante comprobar ,emit(eventType, dato) sin dato y/o sin eventType

Array.prototype._rmv= function rmv (i,e,right) {
  e= this[i];
  right= this.slice(i+1);
  this.length= i;
  while (right.length) this.push(right.shift())
  return e;
}







Array.prototype._has= function has (what,i,ok) {
  i= 0;
  ok= 0;
  while (i < this.length) {
    if (this[i] === what) {
      ok= 1;
      break;
    }
  i++;
  }
  return ok;
}







function rnd (n) {
    return Math.floor(n * Math.random());
}







function rndStr (l, a, str) {
    a = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    str = "";
    while (l--) str += a[rnd(a.length)];
    return str;
}







function boot (eventTypes) {

  function threadWrapListener (eventType) {
    return function threadListener (argv) {
      argv= Array.prototype.splice.call(arguments,0);
      argv.unshift(eventType);
      thread.emit.apply(thread, argv);
    };
  }
  
  eventTypes.forEach(function (v,i,o) {
    thread.on(v, threadWrapListener(v));
  });
  
}






function processWrapListener (eventType, id) {

  return function processListener (argv) {
    argv= Array.prototype.splice.call(arguments,0);
    argv.unshift(eventType);
    checkEvent({thread:this, argv:argv});
  };
  
}






function checkEvent (what,i,ok,str,event,len) {

  i= 0;
  ok= 0;
  str= what.argv.toString();
  while (i < yesSent.length) {
    e= yesSent[i];
    if (e.thread === what.thread) {
      if (e.argv.toString() === str) {
        ok= 1;
        break;
      }
    }
    i++;
  }
  
  if (ok) {
    process.stdout.write("EVENT OK, "+ (--total)+" TO GO         \r");
    yesSent._rmv(i);
  }
  else {
    event= what.argv[0];
    len= what.thread._on[event].length;
    while(t.length) t.pop().destroy();
    console.log("\nTHREAD EMIT DATA EVENTS TEST NOT OK, ERROR!\n");
    while(t.length) t.pop().destroy();
    process.exit(1);
  }
  
  if (yesSent.length || notSent.length) return;
  
  //Hemos acabado!
  console.log("\n\nTHREAD EVENTS STORM TEST ENDED OK, IT WORKS!\n");
  while(t.length) t.pop().destroy();
  
}






function createEvents (events) {
  
  eventTypes.forEach(function (eventType,i,what,j) {
    i= t.length;
    while (i--) {
      t[i].on(eventType, processWrapListener(eventType, t[i].id));
    }
  });
  
  eventTypes.forEach(function (eventType,i,what,j) {
    i= rnd(16);
    while (i--) {
      thread= t[rnd(t.length)];
      what= {thread:thread, argv:[eventType]};
      j= rnd(8);
      while (j--) what.argv.push(rndStr(rnd(8)));
      events.push(what);
      process.stdout.write("CREATED EVENT #"+ events.length+"\r");
    }
  });
  
}






function emitEvents (i,hownamy,what) {

  //1/3 lo mandamos randomly de una tacada
  
  i= 0;
  howMany= notSent.length/2;
  while (++i < howMany) {
    what= notSent._rmv(rnd(notSent.length));
    what.thread.emit.apply(what.thread, what.argv);
    yesSent.push(what);
  }
  
  //Lo que queda se emite randomly en sucesivos nexticks
  
  (function loop (what) {
    if (notSent.length) {
      what= notSent._rmv(rnd(notSent.length));
      what.thread.emit.apply(what.thread, what.argv);
      yesSent.push(what);
      setImmediate(loop);
    }
  })();
  
}






bootDONE= 0;
function threadBootCB (err,res,i,ctr,what) {
  if (err) {
    return process.exit(1);
  }
  
  if (++bootDONE !== t.length) return;
  
  console.log("THREAD BOOT CB: RUN");
  
  createEvents(notSent);
  total= notSent.length;
  process.stdout.write("\n");
  
  //All setup ya solo falta emitir los events
  
  emitEvents();
  
}







function createEventTypes (t,i,n,str) {
  i= 0;
  n= +process.argv[3] || 1024;
  while (i < n) {
    do {
      str= rndStr(rnd(8));
    } while(t._has(str));
    t[i]= str;
    i++;
  }
}







function createThreads (t,i,json,tagg) {
  i= +process.argv[2] || 4;
  json= JSON.stringify(eventTypes);
  tagg= require("threads_a_gogo");
  while (i--) {
    t[i]= tagg.create().eval("("+ boot+ ")("+ json+ ")", threadBootCB);
  }
}






t= [];
total= 0;
notSent= [];
yesSent= [];
eventTypes= [];
createEventTypes(eventTypes);
createThreads(t);

process.stdout.write("EVENTS STORM TEST BEGIN\n");

