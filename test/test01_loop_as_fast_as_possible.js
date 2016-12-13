
var tagg= require('threads_a_gogo');

var i= +process.argv[2] || 3;
process.stdout.write('Using '+ i+ ' threads ');

var threads= [];
while (i--) {
  threads[i]= 0;
  tagg.create().eval(function ƒ () { })
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb)
  .eval('ƒ()', cb);
  process.stdout.write('.');
}
process.stdout.write('\n');

function cb (err, msg) {
  ctr++;
  threads[this.id]++;
  this.eval('ƒ()', cb);
  //process.stdout.write('['+ this.id+ ']');
}

var ctr= 0;
var t= Date.now();

function map (x) { return ( (ctr<10) || (x<10) ) ? '-' : (x/ctr*100).toFixed(1) }

(function display (e,tps) {
  setTimeout(display, 66);
  e= Date.now()- t, tps= (1e3*ctr/e).toFixed(1);
  process.stdout.write('#CB\'s CALLED:'+ ctr+ ', THREADS/SECOND:'+ tps+ ', [ '+ threads.map(map)+ ' ]\r');
})();


process.on('SIGINT', function () {
  console.log('\nBYE !');
  process.exit(0);
});




function pi () {
  var π= 0;
  var num= 4;
  var den= 1;
  var plus= true;

  while (den < 1e6) {
    if (plus) {
      π+= num/den;
      plus= false;
    }
    else {
      π-= num/den;
      plus= true;
    }
    den+= 2;
  }
  return π;
}