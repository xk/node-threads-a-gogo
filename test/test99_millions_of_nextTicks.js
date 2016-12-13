var tagg= require('threads_a_gogo');
var t= tagg.create().eval('('+ f+ ')()', cb).destroy();

function cb (err,str) {
  if (err) console.log('\nCallback says: '+ [err,str]);
}

function f (i,ctr,max) {
  max= 1234567, i= max, ctr= 0;
  thread.nextTick(g);
  function g (n) {
    if (ctr < max) {
      ctr+= 1;
      puts(ctr+ ', _ntq.length: '+ thread._ntq.length+ '     \r');
      //perhaps add a few more nextTick(g)s
      n= i>20 ? 20 : i;
      while (n--) i--, thread.nextTick(g);
    }
    else {
      thread.nextTick(function () {
        //So we can see _ntq.lenght = 1
        puts('\n'+ ctr+ ', _ntq.length: '+ thread._ntq.length+ '     \n');
        puts('I\'M THE LAST ONE, ALL DONE!\n');
      });
    }
  }
}

process.on('exit', function () { console.log('BYE!') });