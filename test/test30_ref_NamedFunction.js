var thread= require('threads_a_gogo').create();
var kN= 25;

function fibo (n) {
  return n > 1 ? fibo(n - 1) + fibo(n - 2) : 1;
}

function cb (err, data) {
  if (err) throw err;
  if (+data !== r) throw Error('FAIL -> RESULT');
  console.log('PASS, OK -> RESULT: ['+ [data, r]+ '], NAME: '+ ref._ref);
  this.destroy();
}

var ref= thread.ref(fibo);

if (ref._ref !== 'thread._refs[0]') throw Error('FAIL -> NAME');

ref(kN, cb);
var r= fibo(kN);
