(function (that) {
  'use strict';
  
  //2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
  //threads_a_gogo load.js
  
  that= this;
  
  function load (path, cb) {
    that.eval(require('fs').readFileSync(path, 'utf8'), cb);
    return that;
  }
  
  that.load= load;
  return load.apply(that, Array.prototype.splice.call(arguments, 0));
})
