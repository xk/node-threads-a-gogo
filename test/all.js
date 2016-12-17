//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo test/all.js
//Quick basic module functionality test

var assert = require('assert');

var tagg= require('threads_a_gogo');
assert(process.versions.threads_a_gogo === '0.1.8');
assert(typeof tagg.create === 'function');
assert(typeof tagg.createPool === 'function');

var str= "hello";
var t=tagg.create().eval("i=100; while(--i); '"+ str+ "';", cb);
assert(typeof t.id === 'number');
assert(typeof t.eval === 'function');
assert(typeof t.emit === 'function');
assert(typeof t.destroy === 'function');
assert(typeof t.on === 'function');
assert(typeof t._on === 'object');
assert(typeof t.load === 'function');
assert(typeof t.removeAllListeners === 'function');
assert(typeof t.version === 'string');

function cb (a,b) {
  assert(!a);
  assert(b === str);
  this.destroy(cb2);
}

function cb2 () {
  assert(1);
}

console.log('THREADS_A_GOGO BASIC FUNCTIONALITY TEST: OK, IT WORKS!');
