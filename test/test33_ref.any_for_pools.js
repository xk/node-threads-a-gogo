var numThreads= parseInt(process.argv[2], 10) || 10;
var pool= require('threads_a_gogo').createPool(numThreads);
var kN= 30;

function fibo (n) {
  return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
}

function cb (err, data) {
  if (err) throw err;
  if (+data !== r) throw Error('FAIL');
  console.log('['+ this.id+ '] ref.any()::PASS, OK -> RESULT: ['+ [data, r]+ '], NAME: '+ ref._ref);
  this.destroy();
}


var ref= pool.ref(fibo);

if (ref._ref !== 'thread._refs[0]') throw Error('FAIL -> NAME');

while (numThreads--) ref.any(kN, cb);
var r= fibo(kN);
