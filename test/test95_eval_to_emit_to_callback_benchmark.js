var tagg= require('threads_a_gogo');
var thread, threads= [];
var  i= +process.argv[2] || 1;
console.log('Using '+ i+ ' threads');

while (i--) {
  thread= tagg.create().on('ping', cb1);
  thread.t0= process.hrtime();
  thread.eval('thread.emit("ping")', cb2);
}

function cb1 () {
  ctr+= 1;
  this.t0= (process.hrtime(this.t0)[1]/1000).toFixed(2)+'(µs)';
  this.t1= process.hrtime();
}

function cb2 () {
  var e= (process.hrtime(this.t1)[1]/1000).toFixed(2)+'(µs)         \r';
  var str= " EVAL TO EMIT CB: "+ this.t0+ " EMIT CB TO EVAL CB: "+ e;
  process.stdout.write('THREAD '+ this.id+ str+ '\r');
  this.t0= process.hrtime();
  this.eval('thread.emit("ping")', cb2);
}

var ctr= 0;
var t= Date.now()-1;
(function display () {
  var e= Date.now()- t;
  var tps= (ctr*1e3/e).toFixed(1);
  console.log('t(ms): '+ e+ ', tps:'+ tps+ ', ctr: '+ ctr+ "                               ");
  setTimeout(display, 888);
})();
