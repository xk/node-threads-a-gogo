tagg= require('threads_a_gogo');
howmany= +process.argv[2] || 4;
process.stdout.write('Using '+ howmany+ ' threads\n');
flipflop= 1;
i= howmany;
ctr= 0;
while (i--) {
  flipflop= flipflop ? 0 : 1;
  t= tagg.create();
  process.stdout.write('THREAD '+ t.id+ ' DESTROY('+ flipflop+ ')\n');
  t.eval('('+ f+ ')()').destroy(flipflop, cb);
}

function f (i) {
  i= 0;
  (function g () {
    (i+= 1)<123456 ? (thread.nextTick(g), puts(i+' THREAD '+ thread.id+ '\r')) : puts(i+' THREAD '+ thread.id+ ' DONE\n');
  })();
}

function cb () {
  ctr+= 1;
  console.log("DESTROY() CALLBACK #"+ ctr);
  if (ctr === howmany) {
    console.log("THIS WAS THE LAST CALLBACK, WILL QUIT IN 5 SECONDS");
    setTimeout(nop, 5000);
  }
}

function nop () {}
