var thread= require('threads_a_gogo').create();
var kN= 25;

var anonymous= function (n) {
  return n > 1 ? arguments.callee(n - 1) + arguments.callee(n - 2) : 1;
};

function cb (err, data) {
  if (err) throw err;
  if (+data !== r) throw Error('FAIL');
  console.log('PASS, OK -> RESULT: ['+ [data, r]+ '], NAME: '+ ref._ref);
  thread.destroy();
}

var ref= thread.ref(anonymous);

if (ref._ref !== 'thread._refs[0]') throw Error('FAIL -> NAME');

ref(kN, cb);
var r= anonymous(kN);
