tagg= require('threads_a_gogo');
howmany= +process.argv[2] || 2;
loops= +process.argv[3] || 2000;
process.stdout.write('Using '+ howmany+ ' threads, '+ loops+ ' loops \n');
while (howmany--) create();

function nop () {}

function create () { tagg.create().eval('thread.nextTick(function (i) { i= 1e5; while (i--) ; })').destroy(0, cb); }

ctr= 0;
function cb () {
  ctr+= 1;
  console.log("DESTROY() CALLBACK #"+ ctr+ "\r");
  if (ctr < loops) create();
}
