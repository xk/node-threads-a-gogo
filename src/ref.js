(function (ctr) {
  
  return function ref (ƒ) {
    
    if (typeof ƒ !== 'function') {
      throw TypeError('.ref( ƒ ) -> ƒ must be a function');
    }
  
    var globalReference= ƒ.name;
    
    if (globalReference) {
      this.eval(ƒ);
    }
    else {
      globalReference= 'thread._refs['+ ctr+ ']';
      ctr++;
      this.eval('!thread._refs && (thread._refs= []);\n'+ globalReference+ '= '+ ƒ);
    }
    
    var that= this;
    function fun () {
      
      var n= arguments.length;
      var params= '()';
      var cb;
    
      if (n) {
        (typeof arguments[n-1] === 'function') && (cb= arguments[--n]);
    
        if (n) {
          params= [];
          for (var i=0 ; i<n ; i++) {
            params[i]= JSON.stringify(arguments[i]);
          }
          params= '('+ params.join(',')+ ')';
        }
      }
    
      return cb ? that.eval(globalReference+ params, cb) : that.eval(globalReference+ params);
    }
    
    fun._ref= globalReference;
    return fun;
  };
  
})(0)
