var i= parseInt(process.argv[2], 10) || 2;
var pool= require('threads_a_gogo').createPool(i);
console.log("Using "+ i+ " threads.");

function ƒ (n) {
  puts(" ["+ thread.id+ "]"+ n);
  //if (!(i%1e3)) gc();
}

var ctr= 0;
function callback (err, data, that, t, n) {
  if (++ctr > 1000) return this.destroy();
  that= this;
  t= Math.floor(60*Math.random());
  if (t<50) {
    that.eval('ƒ('+ctr+')', callback);
  }
  else {
    n= ctr;
    setTimeout(function () { that.eval('ƒ('+n+')', callback) }, t);
  }
}

pool.all.eval('i=0').all.eval(ƒ).all.eval('ƒ()', callback);
