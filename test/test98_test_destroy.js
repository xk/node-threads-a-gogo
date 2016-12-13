tagg= require('threads_a_gogo');
howmany= +process.argv[2] || 10;
process.stdout.write('Using '+ howmany+ ' threads\n');
flipflop= 0;
while (howmany--) {
  flipflop= flipflop ? 0 : 1;
  t= tagg.create();
  process.stdout.write('THREAD '+ t.id+ ' DESTROY('+ flipflop+ ')\n');
  t.eval('('+ f+ ')()').destroy(flipflop);
}

function f (i) {
  i= 0;
  (function g () {
    (i+= 1)<123456 ? (thread.nextTick(g), puts(i+' THREAD '+ thread.id+ '\r')) : puts(i+' THREAD '+ thread.id+ ' DONE\n');
  })();
}
