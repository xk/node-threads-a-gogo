

var tagg= require('threads_a_gogo');


var i= 0;
(function again () {
  i++;
  tagg.create().destroy(1, again);
})();


var t= Date.now();
(function display () {
  var e= Date.now()- t;
  var tps= (i*1e3/e).toFixed(1);
  process.stdout.write('\nt (ms) -> '+ e+ ', i -> '+ i+ ', created/destroyed-per-second -> '+ tps);
  setTimeout(display, 666);
})();
