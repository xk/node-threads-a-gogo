tagg= require('threads_a_gogo');
howmany= +process.argv[2] || 4;
process.stdout.write('Using '+ howmany+ ' threads\n');

while (howmany--) {
  tagg.create().eval('('+ f+ ')()').destroy();
}


function f (i, ctr) {
  i= 1234567, ctr= 0;
  thread._ntq.push(g);
  function g () {
    if (i--) ctr+= 1, thread._ntq.push(g);
    else puts(ctr+' DONE!\n')
  }
}
