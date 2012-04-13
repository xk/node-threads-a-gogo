var thread= require('threads_a_gogo').create();
var kN= 25;

function fibo (n) {
  return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
}

function cb (err, data) {
  if (err) throw err;
  if (+data !== r) throw Error('FAIL -> RESULT');
  console.log('PASS, OK -> RESULT: ['+ [data, r]+ '], NAME: '+ ref._ref);
  thread.destroy();
}

var ref= thread.ref(fibo);

if (ref._ref !== fibo.name) throw Error('FAIL -> NAME');

ref(kN, cb);
var r= fibo(kN);
