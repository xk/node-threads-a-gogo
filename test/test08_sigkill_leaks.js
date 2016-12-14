var tagg= require('threads_a_gogo');

function cb (e,m) {
  process.nextTick(again);
  console.log('['+this.id+'].destroy()');
  this.destroy();
}


function again () {
  tagg.create().eval('0', cb);
}


var i= +process.argv[2] || 2;
console.log('Using '+ i+ ' threads');



while (i--) again();


process.on('exit', function () {
  console.log("process.on('exit') -> BYE!");
});
