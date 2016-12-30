//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo test/00_events_storm.js

//Esto emite unos cuantos miles de eventos de distintos tipos y con distintos datos y longitudes
//Y se asegura de que los listeners reciben correctamente lo enviado.
//Es importante comprobar ,emit(eventType, dato) sin dato y/o sin eventType

Array.prototype._rmv= function rmv (i) {
  return this.splice(i,1);
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






function checkEvent (what,i,ok,str,e,event,len,nuArray) {

  i= 0;
  ok= 0;
  str= what.argv.toString();
  while (i < sentEvents.length) {
    e= sentEvents[i];
    if (e.thread === what.thread) {
      if (e.argv.toString() === str) {
        ok= 1;
        sentEvents._rmv(i);
        break;
      }
    }
    i++;
  }
  
  if (ok) {
    total+= 1;
    process.stdout.write("EVENT OK, THREAD "+ e.thread.id+ ", #"+ total+"  \r");
  }
  else {
    event= what.argv[0];
    len= what.thread._on[event].length;
    while(t.length) t.pop().destroy();
    console.log("\nTHREAD EMIT DATA EVENTS TEST NOT OK, ERROR!\n");
    while(t.length) t.pop().destroy();
    process.exit(1);
  }
  
  if (total <  howManyEvents) return;
  
  //Hemos acabado!
  console.log("\n\nTHREAD EVENTS STORM TEST ENDED OK, IT WORKS!\n");
  while(t.length) t.pop().destroy();
  
}






function createListeners (events,thread,event) {
  eventTypes.forEach(function (eventType,i) {
    i= t.length;
    while (i--) {
      t[i].on(eventType, processWrapListener(eventType, t[i].id));
    }
  });
}






function createRandomEvent (thread,eventType,what,i) {
  
  thread= t[rnd(t.length)];
  eventType= eventTypes[rnd(eventTypes.length)];
  what= {thread:thread, argv:[eventType]};
  i= rnd(8);
  while (i--) what.argv.push(rndStr(rnd(8)));
  return what;
  
}






function emitEvents (i,howMany,what,n) {

  //Hasta 20000 los mandamos randomly de una tacada
  
  i= 0;
  howMany= Math.floor(howManyEvents/2);
  howMany= (howMany > 20e3) ? 20e3 : howMany;
  while (i < howMany) {
    what= createRandomEvent();
    sentEvents.push(what);
    what.thread.emit.apply(what.thread, what.argv);
    i+= 1;
  }
  
  //Se emite randomly en sucesivos nexticks
  
  howMany= howManyEvents- howMany;
  (function loop (what,i) {
    if (howMany > 0) {
      howMany-= 1;
      what= createRandomEvent();
      sentEvents.push(what);
      what.thread.emit.apply(what.thread, what.argv);
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
  
  process.stdout.write("USING "+ howManyThreads+ " THREADS, ");
  process.stdout.write(howManyEvents+ " EVENTS, ");
  process.stdout.write(howManyEventTypes+ " EVENTTYPES\n");
  
  console.log("THREAD BOOT CB: RUN");
  
  createListeners();
  
  //All setup ya solo falta emitir los events
  
  emitEvents();
  
}







function createEventTypes (t,i,str) {
  i= 0;
  while (i < howManyEventTypes) {
    do {
      str= rndStr(rnd(8));
    } while(t._has(str));
    t[i]= str;
    i++;
  }
}







function createThreads (t,i,json,tagg) {
  i= howManyThreads;
  json= JSON.stringify(eventTypes);
  tagg= require("threads_a_gogo");
  while (i--) {
    t[i]= tagg.create().eval("("+ boot+ ")("+ json+ ")", threadBootCB);
  }
}






t= [];
total= 0;
sentEvents= [];
eventTypes= [];
howManyThreads= +process.argv[2] || 2;
howManyEvents= +process.argv[3] || 100e3;
howManyEventTypes= +process.argv[4] || 128;
createEventTypes(eventTypes);
createThreads(t);

process.stdout.write("EVENTS STORM TEST BEGIN\n");

