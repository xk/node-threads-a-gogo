var tagg= require('threads_a_gogo');
var i,t,e,et= 0;
var cuantas= +process.argv[2] || 20;
console.log("Lanzando tagg.create().destroy(0,cb) en lotes de "+ cuantas);

function create () {
  i= cuantas;
  t= Date.now();
  while (i--) tagg.create().eval('('+ f+ ')()').destroy(0,cb);
}

function f () {
  function nop () {}
  thread.nextTick(nop);
}

var ctr= 0;
var total= 0;
function cb () {
  ctr+= 1;
  if (ctr === cuantas) {
    e= Date.now()- t;
    et+= e;
    total+= ctr;
    ctr= 0;
    setTimeout(create,333+e);
    var tpsi= (cuantas*1e3/e).toFixed(1)+ " INSTANTANEO, ";
    var tpsa= (total*1e3/et).toFixed(1)+ " AVERAGE\r";
    process.stdout.write('['+ total+ '] '+ tpsi+ tpsa);
  }
}

create();
