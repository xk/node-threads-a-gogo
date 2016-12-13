

var tagg= require('threads_a_gogo');

var i= +process.argv[2] || 1;
process.stdout.write('Using '+ i+ ' threads ');

while (i--) {
  tagg.create().eval(function ƒ () { }).eval('ƒ()', cb);
  process.stdout.write('.');
}
process.stdout.write('\n');

function cb (err, msg) {
  ctr+= 1;
  this.eval('ƒ()', cb);
}

var ctr= 0;
var t= Date.now();
(function display (e,tps) {
  setTimeout(display, 333);
  e= Date.now()- t, tps= (1e3*ctr/e).toFixed(1);
  process.stdout.write('#CALLBACKS CALLED: '+ ctr+ ', THREADS PER SECOND: '+ tps+ '\r');
})();

process.on('SIGINT', function () {
  console.log('\nBYE !');
  process.exit(0);
});
