

var T= require('../lib');


var i= 0;
var k= 5;
(function again () {
  var j= k;
  while (j--) {
    T.create().destroy();
  }
  i+= k;
  process.nextTick(again);
})();


var t= Date.now();
function display () {
  var e= Date.now()- t;
  var tps= (i*1e3/e).toFixed(1);
  process.stdout.write('\nt (ms) -> '+ e+ ', i -> '+ i+ ', created/destroyed-per-second -> '+ tps);
}

setInterval(display, 1e3);
