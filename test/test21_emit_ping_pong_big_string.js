

var i= process.argv[2] || 23; //8MB
if (process.argv[2]) {
  process.stdout.write("Usando 2^"+ i);
}
else {
  process.stdout.write("No hay primer argumento, usando 23: 2^23 ~= 8MB");
}
var big= "*";
while (i--) big+= big;
console.log(". La string.length es: "+ big.length+ ' bytes');

var i= 0;
var s= Date.now();
var o= require('threads_a_gogo')
  .create()
  .eval(function boot () {
    thread.on('b', function (data) {
      this.emit('a',data);
    });
  })
  .eval('boot()')
  .emit('b',big)
  .on('a', function (data) {
    this.emit('b',data);
    i+= 1;
  });


function display () {
  var e= Date.now()- s;
  var ppps= (i*1e3/e).toFixed(1);
  var t= (1e6/+ppps);
  if (t>1000) {
    t= (t/1000).toFixed(2)+" ms";
  }
  else t= t.toFixed(1)+ " µs";  
  process.stdout.write("ping-pongs: "+ i+ ", ping-pongs-per-second: "+ ppps+ ", "+ t+ "      \r");
}

setInterval(display, 333);
