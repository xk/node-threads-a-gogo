//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo test/all.js
//Quick basic module functionality test
//*TODO* test events .emit .on y .once y pools y nextTicks y seImmediate y puts

var assert = require('assert');

var tagg= require('threads_a_gogo');
process.stdout.write('0.OK.');
assert.equal(process.versions.threads_a_gogo, '0.1.8');
process.stdout.write('1.OK.');
assert.equal(typeof tagg.create, 'function');
process.stdout.write('2.OK.');
assert.equal(typeof tagg.createPool, 'function');
process.stdout.write('3.OK.');
var str= "hello";
var t=tagg.create().eval("i=100; while(--i); '"+ str+ "';", cb);
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
process.stdout.write('12.OK.');
assert.equal(typeof t.version, 'string');
process.stdout.write('13.OK.WAITING FOR CB.');

function cb (a,b) {
  process.stdout.write('14.OK.');
  assert.equal(!!a, false);
  process.stdout.write('15.OK.');
  assert.equal(b, str);
  process.stdout.write('16.OK.');
  this.destroy(0,cb2);
  process.stdout.write('17.OK.WAITING FOR CB.');
}

function cb2 () {
  process.stdout.write('19.OK.END\n');
  process.stdout.write('THREADS_A_GOGO BASIC FUNCTIONALITY TEST: OK, IT WORKS!\n');
}
