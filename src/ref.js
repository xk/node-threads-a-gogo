(function ref (ƒ) {
  
  if (typeof ƒ !== 'function') {
    throw TypeError('.ref( ƒ ) -> ƒ must be a function');
  }
  
  var name= ƒ.name;
  if (!name) {
    throw TypeError('.ref( ƒ ) -> ƒ is anonymous');
  }
  
  if (!this.call) {
    this.call= Object.create(null);
  }
  
  var that= this;
  this.call[name]= function () {
    var n= arguments.length;
    var params= '()';
    var cb;
    
    if (n) {
      if (typeof arguments[n-1] === 'function') {
        cb= arguments[--n];
      }
    
      if (n) {
        params= [];
        for (var i=0 ; i<n ; i++) {
          params[i]= JSON.stringify(arguments[i]);
        }
        params= '('+ params.join(',')+ ')';
      }
    }
    
    return cb ? that.eval(name+ params, cb) : that.eval(name+ params);
  }
  
  return this.eval(ƒ);
})