(function (ctr) {
  
  return function ref (ƒ) {
    if (typeof ƒ !== 'function') {
      throw TypeError('.ref( ƒ ) -> ƒ must be a function');
    }
    
    var that= this;
    var globalReference= 'thread._refs['+ ctr+ ']';
    if (ctr++) {
      that.eval(globalReference+ '= '+ ƒ);
    }
    else {
      that.eval('thread._refs= ['+ ƒ+ ']');
    }
    
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
    
      cb ? that.eval(globalReference+ params, cb) : that.eval(globalReference+ params);
      return fun;
    }
    
    fun._ref= globalReference;
    return fun;
  };
  
})(0)
