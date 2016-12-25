//2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo test/01_basic.js

//Quick basic module functionality test
//*TODO* test events .emit .once y pools y nextTicks y seImmediate y puts

var assert = require('assert');

var steps= 0;
function step (msg) {
  if (!msg) msg= "OK.";
  process.stdout.write(steps+ '.'+ msg);
  steps+= 1;
}

function rndStr(l, a, str) {
    a = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    str = "";
    while (l--) str += a[Math.floor(a.length * Math.random())];
    return str;
}

var tagg= require('threads_a_gogo');
step();
assert.equal(typeof tagg.create, 'function');
step();
assert.equal(typeof tagg.createPool, 'function');
step('TAGG OBJECT OK\n');

function boot () {
  thread.on('hello', function f (a,b,c) {
    thread.emit('hello', thread.id, this.id, a, b, c);
  });
  return 'to_eval_cb';
}

var name= rndStr(12)+ '.tagg.test.js';
var path= (process.env.TMPDIR || '/tmp/')+ name;
require('fs').writeFileSync(path, boot);

var t= tagg.create().load(path, cb).eval("boot()", cb1);
step();
assert.equal(typeof t.id, 'number');
step();
assert.equal(typeof t.eval, 'function');
step();
assert.equal(typeof t.emit, 'function');
step();
assert.equal(typeof t.destroy, 'function');
step();
assert.equal(typeof t.on, 'function');
step();
assert.equal(typeof t._on, 'object');
step();
assert.equal(typeof t.load, 'function');
step();
assert.equal(typeof t.removeAllListeners, 'function');
step('THREAD OBJECT OK\n');
assert.equal(typeof t.version, 'string');
step('OK.WAITING FOR LOAD CB\n');

function cb (a) {
  assert.equal(t.id, this.id);
  step();
  assert.equal(!!a, false);
  step('LOAD CALLBACK OK\n');
  step('OK.WAITING FOR EVAL CB\n');
}

function cb1 (a,b) {
  assert.equal(t.id, this.id);
  step();
  assert.equal(!!a, false);
  step();
  assert.equal(b, 'to_eval_cb');
  step('EVAL CALLBACK OK\n');
  this.on('hello', cb2).emit('hello','hello','tagg','world');
  step('OK.WAITING FOR EVENT LISTENER CB\n');
}

function cb2 (tid1,tid2,a,b,c) {
  assert.equal(t.id, this.id);
  step();
  assert.equal(+tid1, t.id);
  step();
  assert.equal(+tid2, t.id);
  step();
  assert.equal(a, 'hello');
  step();
  assert.equal(b, 'tagg');
  step();
  assert.equal(c, 'world');
  step('EVENT LISTENER CB.OK\n');
  this.destroy(0,cb3);
  step('OK.WAITING FOR DESTROY CB\n');
}

function cb3 () {
  step();
  assert.equal(this, global);
  step('DESTROY CB OK\nEND\n');
  process.stdout.write('THREADS_A_GOGO v'
               + tagg.version
               + ' BASIC FUNCTIONALITY TEST: OK, IT WORKS!\n');
}
