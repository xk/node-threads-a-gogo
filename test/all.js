//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo test/all.js
//Quick basic module functionality test
//*TODO* test events .emit .on y .once y pools y nextTicks y seImmediate y puts

var assert = require('assert');

var tagg= require('threads_a_gogo');
process.stdout.write('0.OK.');
assert.equal(typeof process.versions.threads_a_gogo, 'string');
process.stdout.write('1.OK.');
assert.equal(typeof tagg.create, 'function');
process.stdout.write('2.OK.');
assert.equal(typeof tagg.createPool, 'function');
process.stdout.write('3.TAGG OBJECT OK\n');
function boot () {
  thread.on('hello', function f (a,b,c) {
    thread.emit('hello', thread.id, this.id, a, b, c);
  });
  return 'to_eval_cb';
}
var t= tagg.create().eval("("+ boot+ ")()", cb);
process.stdout.write('4.OK.');
assert.equal(typeof t.id, 'number');
process.stdout.write('5.OK.');
assert.equal(typeof t.eval, 'function');
process.stdout.write('6.OK.');
assert.equal(typeof t.emit, 'function');
process.stdout.write('7.OK.');
assert.equal(typeof t.destroy, 'function');
process.stdout.write('8.OK.');
assert.equal(typeof t.on, 'function');
process.stdout.write('9.OK.');
assert.equal(typeof t._on, 'object');
process.stdout.write('10.OK.');
assert.equal(typeof t.load, 'function');
process.stdout.write('11.OK.');
assert.equal(typeof t.removeAllListeners, 'function');
process.stdout.write('12.THREAD OBJECT OK\n');
assert.equal(typeof t.version, 'string');
process.stdout.write('13.OK.WAITING FOR EVAL CB\n');

function cb (a,b) {
  assert.equal(t.id, this.id);
  process.stdout.write('14.OK.');
  assert.equal(!!a, false);
  process.stdout.write('15.OK.');
  assert.equal(b, 'to_eval_cb');
  process.stdout.write('16.EVAL CALLBACK OK\n');
  this.on('hello', cb2).emit('hello','hello','tagg','world');
  process.stdout.write('17.OK.WAITING FOR EVENT LISTENER CB\n');
}

function cb2 (tid1,tid2,a,b,c) {
  assert.equal(t.id, this.id);
  process.stdout.write('18.OK.');
  assert.equal(+tid1, t.id);
  process.stdout.write('19.OK.');
  assert.equal(+tid2, t.id);
  process.stdout.write('20.OK.');
  assert.equal(a, 'hello');
  process.stdout.write('21.OK.');
  assert.equal(b, 'tagg');
  process.stdout.write('22.OK.');
  assert.equal(c, 'world');
  process.stdout.write('23.EVENT LISTENER CB.OK\n');
  this.destroy(0,cb3);
  process.stdout.write('24.OK.WAITING FOR DESTROY CB\n');
}

function cb3 () {
  process.stdout.write('25.OK.');
  assert.equal(this, global);
  process.stdout.write('26.DESTROY CB OK\nEND\n');
  process.stdout.write('THREADS_A_GOGO v'
               + process.versions.threads_a_gogo
               + ' BASIC FUNCTIONALITY TEST: OK, IT WORKS!\n');
}
