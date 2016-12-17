var tagg= require('threads_a_gogo');

function cb (e,m) {
  setImmediate(create);
  process.stdout.write('['+this.id+'].destroy()\r');
  this.destroy();
}

function create () {
  tagg.create().eval('0', cb);
}

var i= +process.argv[2] || 2;
console.log('Using '+ i+ ' threads');
while (i--) create();

process.on('exit', function () {
  console.log("process.on('exit') -> BYE!");
});
