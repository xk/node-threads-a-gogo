//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo test/00_emit_events_data.js

//Esto emite unos cuantos miles de eventos de distintos tipos y con distintos datos y longitudes
//Y se asegura de que los listeners reciben correctamente lo enviado.

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
      //puts("\x07");
      //puts("THREAD"+ thread.id+ " CB FOR EVENT: "+ argv+ "\n");
    };
  }
  
  eventTypes.forEach(function (v,i,o) {
    //puts("THREAD"+ thread.id+ " SET CB FOR EVENT: "+ v+ "\n");
    thread.on(v, threadWrapListener(v));
  });
  
}






function processWrapListener (eventType, id) {

  //process.stdout.write("PROCESS WRAP CB FOR THREAD "+ id+ " : "+ eventType+ "\n");
  
  return function processListener (argv) {
    //process.stdout.write("\x07");
    argv= Array.prototype.splice.call(arguments,0);
    argv.unshift(eventType);
    checkEvent({thread:this, argv:argv});
  };
  
}






function checkEvent (what,i,e,str,ok,event,len) {

  process.stdout.write("PROCESS GOT EVENT FROM THREAD "+ what.thread.id+ " : "+ what.argv);
  process.stdout.write("                                \r");
  
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
    yesSent._rmv(i);
    //process.stdout.write("EVENT FROM THREAD "+ e.thread.id+ " MATCH OK: "+ str+ "\n");
  }
  else {
    event= what.argv[0];
    len= what.thread._on[event].length;
    //process.stdout.write("***** EVENT FROM THREAD "+ e.thread.id+ " MATCH NOT OK: "+ str+ " Q.LEN: "+ len+ "\x07\x07\n");
    while(t.length) t.pop().destroy();
    console.log("\nTHREAD EMIT DATA EVENTS TEST NOT OK, ERROR!\n");
    process.exit(1);
  }
  
  if (yesSent.length || notSent.length) return;
  
  //Hemos acabado!
  console.log("\n\nTHREAD EMIT DATA EVENTS TEST OK, IT WORKS!\n");
  while(t.length) t.pop().destroy();
  
}






function createEvents (ctr) {
  ctr= 0;
  eventTypes.forEach(function (eventType,i,what,j) {
    i= t.length;
    while (i--) {
      //Only one listener per eventType, please.
      t[i].on(eventType, processWrapListener(eventType, t[i].id));
      //process.stdout.write("PROCESS SET CB FOR THREAD "+ t[i].id+ " : "+ eventType+ "\n");
    }
  });
  
  //process.stdout.write("\n");
  
  eventTypes.forEach(function (eventType,i,what,j) {
    i= 1+ rnd(16);
    while (i--) {
      thread= t[rnd(t.length)];
      what= {thread:thread, argv:[eventType]};
      j= 1+ rnd(8);
      while (j--) what.argv.push(1+rndStr(rnd(8)));
      notSent.push(what);
      //process.stdout.write("PROCESS CREATING EVENT FOR THREAD "+ thread.id+ " #"+ (++ctr)+ " "+ what.argv+ "\n");
    }
  });
  
}






bootDONE= 0;
function bootcb (err,res,howMany,i,what,ctr) {
  if (err) {
    //console.log("BOOTCB: ",err,res);
    return process.exit(1);
  }
  
  if (++bootDONE !== t.length){
    //console.log("BOOTCB: NOT YET...");
    return;
  }
  
  console.log("BOOTCB: RUN NOW");
  
  createEvents();
  
  //All is setup but nothing has been sent/emitted yet
  //Lets send half of them randomly right now in a single batch
  
  //process.stdout.write("\n");
  
  i= 0;
  ctr= 0;
  howMany= notSent.length/2;
  while (++i < howMany) {
    what= notSent._rmv(rnd(notSent.length));
    //process.stdout.write("PROCESS SENDING EVENT TO THREAD "+ what.thread.id+ " #"+ (++ctr)+ " : "+ what.argv+"\n");
    what.thread.emit.apply(what.thread, what.argv);
    yesSent.push(what);
  }
  
  //process.stdout.write("\n");
  
  //What is left we send it randomly in nexticks
  (function loop (what) {
    if (notSent.length) {
      what= notSent._rmv(rnd(notSent.length));
      what.thread.emit.apply(what.thread, what.argv);
      yesSent.push(what);
      //process.stdout.write("PROCESS NEXTTICK SENDING EVENT TO THREAD "+ what.thread.id+ " #"+ (++ctr)+ " : "+ what.argv+"\n");
      setImmediate(loop);
    }
  })();
}








i= 0;
eventTypes= [];
n= +process.argv[3] || 128;
while (i < n) {
  do {
    str= rndStr(1+rnd(8));
  } while(eventTypes._has(str));
  eventTypes[i++]= str;
}
json= JSON.stringify(eventTypes);







DEBUG= 0;
t= [];
notSent= [];
yesSent= [];
i= +process.argv[2] || 2;
tagg= require("threads_a_gogo");
while (i--) {
  t[i]= tagg.create().eval("("+ boot+ ")("+ json+ ")", bootcb);
}






//process.stdout.write("\n");

