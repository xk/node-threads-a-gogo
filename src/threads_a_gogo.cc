//2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo.cc

#include <v8.h>
#include <node.h>
#include <node_version.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string>

static int DEBUG= 0;

#define kThreadMagicCookie 0x99c0ffee

#define TAGG_USE_LIBUV
#if (NODE_MAJOR_VERSION == 0) && (NODE_MINOR_VERSION <= 5)
#undef TAGG_USE_LIBUV
#endif

#ifdef TAGG_USE_LIBUV
  #define WAKEUP_EVENT_LOOP uv_async_send(&thread->async_watcher);
#else
  #define WAKEUP_EVENT_LOOP ev_async_send(EV_DEFAULT_UC_ &thread->async_watcher);
#endif

//using namespace node;
using namespace v8;

typedef enum jobTypes {
  kJobTypeEval,
  kJobTypeEvent
} jobTypes;

typedef struct typeEvent {
  int length;
  String::Utf8Value* eventName;
  String::Utf8Value** argumentos;
} typeEvent;

typedef struct typeEval {
  int error;
  int tiene_callBack;
  int useStringObject;
  String::Utf8Value* resultado;
  union {
    char* scriptText_CharPtr;
    String::Utf8Value* scriptText_StringObject;
  };
} typeEval;
     
typedef struct typeJob {
  int jobType;
  Persistent<Object> cb;
  union {
    typeEval eval;
    typeEvent event;
  };
} typeJob;

typedef struct typeQueueItem {
  typeQueueItem* next;
  typeJob job;
} typeQueueItem;

typedef struct typeQueue {
  typeQueueItem* last;
  typeQueueItem* first;
  pthread_mutex_t queueLock;
} typeQueue;

typedef struct typeThread {

#ifdef TAGG_USE_LIBUV
  uv_async_t async_watcher; //MUST be the first one
#else
  ev_async async_watcher; //MUST be the first one
#endif
  
  long int id;
  pthread_t thread;
  volatile int ended;
  volatile int sigkill;
  volatile int sigkillNicely;
  
  typeQueue processToThreadQueue;  //Jobs to run in the thread
  typeQueue threadToProcessQueue;  //Jobs to run in node's main thread
  
  volatile int IDLE;
  pthread_cond_t IDLE_cv;
  
  Isolate* isolate;
  Persistent<Context> context;
  Persistent<Object> JSObject;
  Persistent<Object> threadJSObject;
  Persistent<Object> dispatchEvents;
  
  unsigned long threadMagicCookie;
} typeThread;

#include "queues_a_gogo.cc"

static bool useLocker;
static long int threadsCtr= 0;
static Persistent<String> id_symbol;
static typeQueue* freeJobsQueue= NULL;
static Persistent<ObjectTemplate> threadTemplate;


/*

cd deps/minifier/src
gcc minify.c -o minify
cat ../../../src/events.js | ./minify kEvents_js > ../../../src/kEvents_js
cat ../../../src/load.js | ./minify kLoad_js > ../../../src/kLoad_js
cat ../../../src/createPool.js | ./minify kCreatePool_js > ../../../src/kCreatePool_js
cat ../../../src/nextTick.js | ./minify kNextTick_js > ../../../src/kNextTick_js

*/

#include "events.js.c"
#include "createPool.js.c"
#include "nextTick.js.c"

//node-waf configure uninstall distclean configure build install








static typeQueueItem* nuJobQueueItem (void) {
  typeQueueItem* qitem= qPull(freeJobsQueue);
  if (!qitem) qitem= nuQitem();
  return qitem;
}






static typeThread* isAThread (Handle<Object> receiver) {
  typeThread* thread;
  if (receiver->IsObject()) {
    if (receiver->InternalFieldCount() == 1) {
      thread= (typeThread*) receiver->GetPointerFromInternalField(0);
      if (thread && (thread->threadMagicCookie == kThreadMagicCookie)) {
        return thread;
      }
    }
  }
  return NULL;
}






static void pushJobToThread (typeQueueItem* qitem, typeThread* thread) {

//Esto se ejecuta siempre en node's main thread

  DEBUG && printf("PUSH JOB TO THREAD #1\n");
  //Esto garantiza que haya algo en la queue porque si no
  //la thread podría echarse a dormir (tb podría ya estar durmiendo)
  //si ve que no hay nada en la queue
  qPush(qitem, &thread->processToThreadQueue);
  
  //Cogiendo este lock sabemos que la thread o no ha salido aún
  //del event loop o está parada en wait/sleep/idle
  pthread_mutex_lock(&thread->processToThreadQueue.queueLock);
  
  DEBUG && printf("PUSH JOB TO THREAD #2\n");
  //Estamos seguros de que no se está tocando thread->IDLE
  //xq tenemos el lock nosotros y sólo se toca con el lock puesto
  if (thread->IDLE) {
    //estaba parada, hay que ponerla en marcha
    DEBUG && printf("PUSH JOB TO THREAD #3\n");
    pthread_cond_signal(&thread->IDLE_cv);
  }
  DEBUG && printf("PUSH JOB TO THREAD #4\n");
  //Hay que volver a soltar el lock
  pthread_mutex_unlock(&thread->processToThreadQueue.queueLock);
  DEBUG && printf("PUSH JOB TO THREAD #5\n");
  DEBUG && printf("PUSH JOB TO THREAD #6\n");
}






static Handle<Value> Puts (const Arguments &args) {
  HandleScope scope;
  int i= 0;
  while (i < args.Length()) {
    String::Utf8Value c_str(args[i]);
    fputs(*c_str, stdout);
    i++;
  }
  fflush(stdout);
  return Undefined();
}





static void eventLoop (typeThread* thread);







static void* threadBootProc (void* arg) {

//Esto es lo primero que se ejecuta en la(s) thread(s) al nacer.
//Básicamente inicializa lo necesario y se entra en el eventloop

  int dummy;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &dummy);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &dummy);
  
  typeThread* thread= (typeThread*) arg;
  thread->isolate= Isolate::New();
  thread->isolate->SetData(thread);
  
  if (useLocker) {
    //DEBUG && printf("**** USING LOCKER: YES\n");
    v8::Locker myLocker(thread->isolate);
    //v8::Isolate::Scope isolate_scope(thread->isolate);
    eventLoop(thread);
  }
  else {
    eventLoop(thread);
  }
  
  DEBUG && printf("THREAD %ld BOOT EXIT #1\n", thread->id);
  thread->isolate->Exit();
  DEBUG && printf("THREAD %ld BOOT EXIT #2\n", thread->id);
  thread->isolate->Dispose();
  DEBUG && printf("THREAD %ld BOOT EXIT #3\n", thread->id);
  thread->ended= 1;
  WAKEUP_EVENT_LOOP
  DEBUG && printf("THREAD %ld BOOT EXIT #4\n", thread->id);
  return NULL;
}





static Handle<Value> threadEmit (const Arguments &args);






static void eventLoop (typeThread* thread) {

// The thread's eventloop runs in the thread(s) not in node's main thread

  thread->isolate->Enter();
  thread->context= Context::New();
  thread->context->Enter();
  
  {
    HandleScope scope1;
    
    Local<Object> global= thread->context->Global();
    global->Set(String::NewSymbol("puts"), FunctionTemplate::New(Puts)->GetFunction());
    Local<Object> threadObject= Object::New();
    global->Set(String::NewSymbol("thread"), threadObject);
    threadObject->Set(String::NewSymbol("id"), Number::New(thread->id));
    threadObject->Set(String::NewSymbol("emit"), FunctionTemplate::New(threadEmit)->GetFunction());
    Local<Object> dispatchEvents= Script::Compile(String::New(kEvents_js))->Run()->ToObject()->CallAsFunction(threadObject, 0, NULL)->ToObject();
    Local<Object> dispatchNextTicks= Script::Compile(String::New(kNextTick_js))->Run()->ToObject()->CallAsFunction(threadObject, 0, NULL)->ToObject();
    Local<Array> _ntq= (v8::Array*) *threadObject->Get(String::NewSymbol("_ntq"));
    long int ctr= 0;
    long int kGC= 1000;
    
    //SetFatalErrorHandler(FatalErrorCB);
    
    while (!thread->sigkill) {
      typeJob* job;
      double ntql= 0;
      typeQueueItem* qitem;
      
      {
        HandleScope scope2;
        TryCatch onError;
        String::Utf8Value* str;
        Local<String> source;
        Local<Script> script;
        Local<Value> resultado;
        
        DEBUG && printf("THREAD %ld BEFORE WHILE\n", thread->id);
        
        while ((qitem= qPull(&thread->processToThreadQueue)) || (ntql= _ntq->Length())) {
          
          DEBUG && printf("THREAD %ld WHILE\n", thread->id);
          
          if (thread->sigkill) break;
          
          if ((++ctr) > kGC) {
            ctr= 0;		
            V8::IdleNotification();		
          }
          
          if (ntql) {
            DEBUG && printf("THREAD %ld NTQL\n", thread->id);
            dispatchNextTicks->CallAsFunction(threadObject, 0, NULL);
            _ntq= (v8::Array*) *threadObject->Get(String::NewSymbol("_ntq"));
            if (onError.HasCaught()) onError.Reset();
          }
          else
            DEBUG && printf("THREAD %ld NO NTQL\n", thread->id);
        
          if (thread->sigkill) break;
          
          if ((++ctr) > kGC) {
            ctr= 0;		
            V8::IdleNotification();		
          }
          
          if (qitem) {
            DEBUG && printf("THREAD %ld QITEM\n", thread->id);
            job= &qitem->job;
            if (job->jobType == kJobTypeEval) {
              //Ejecutar un texto
            
              if (job->eval.useStringObject) {
                str= job->eval.scriptText_StringObject;
                source= String::New(**str, (*str).length());
                delete str;
              }
              else {
                source= String::New(job->eval.scriptText_CharPtr);
                free(job->eval.scriptText_CharPtr);
              }
            
              script= Script::New(source);
            
              if (!onError.HasCaught()) resultado= script->Run();

              if (job->eval.tiene_callBack) {
                job->eval.error= onError.HasCaught() ? 1 : 0;
                job->eval.resultado= new String::Utf8Value(job->eval.error ? onError.Exception() : resultado);
                qPush(qitem, &thread->threadToProcessQueue);
                WAKEUP_EVENT_LOOP
              }
              else {
                qPush(qitem, freeJobsQueue);
              }

              if (onError.HasCaught()) onError.Reset();
            }
            else if (job->jobType == kJobTypeEvent) {
              //Emitir evento.
            
              Local<Value> args[2];
              str= job->event.eventName;
              args[0]= String::New(**str, (*str).length());
              delete str;
            
              Local<Array> array= Array::New(job->event.length);
              args[1]= array;
            
              int i= 0;
              while (i < job->event.length) {
                str= job->event.argumentos[i];
                array->Set(i, String::New(**str, (*str).length()));
                delete str;
                i++;
              }
            
              free(job->event.argumentos);
              qPush(qitem, freeJobsQueue);
              dispatchEvents->CallAsFunction(global, 2, args);
            }
          }
          else
            DEBUG && printf("THREAD %ld NO QITEM\n", thread->id);
          
        }
        
      }
      
      //Si nos han pedido amablemente que acabemos y no había nada que hacer...
      //entonces activar sigkill aquí sería hacerlo nicely not rudely
      if (thread->sigkillNicely) thread->sigkill= 1;
      if (thread->sigkill) break;

      V8::IdleNotification();
      
      DEBUG && printf("THREAD %ld BEFORE MUTEX\n", thread->id);
      //cogemos el lock para
      //por un lado poder mirar si hay cosas en la queue sabiendo
      //que nadie la está tocando
      //y por otro lado para poder tocar thread->IDLE sabiendo
      //que nadie la está mirando mientras la tocamos.
      pthread_mutex_lock(&thread->processToThreadQueue.queueLock);
      DEBUG && printf("THREAD %ld TIENE processToThreadQueue_MUTEX\n", thread->id);
      //aquí tenemos acceso exclusivo a processToThreadQueue y a thread->IDLE
      while (!thread->processToThreadQueue.first && !thread->sigkill) {
        //sólo se entra aquí si no hay nada en la queue y no hay sigkill
        //hemos avisado con thread->IDLE de que nos quedamos parados
        // para que sepan que nos han de despertar
        thread->IDLE= 1;
        DEBUG && printf("THREAD %ld SLEEP\n", thread->id);
        //en pthread_cond_wait se quedará atascada esta thread hasta que
        //nos despierten y haya cosas en la queue o haya sigkill
        //El lock se abre al entrar en pthread_cond_wait así que los
        //demás ahora van a poder mirar thread->IDLE mientras estamos parados/durmiendo
        pthread_cond_wait(&thread->IDLE_cv, &thread->processToThreadQueue.queueLock);
        //El lock queda cerrado al salir de pthread_cond_wait pero no importa xq
        //si seguimos en el bucle se va a volver a abrir y si salimos tb
      }
      //Aquí aún tenemos el lock así que podemos tocar thread->IDLE con seguridad
      thread->IDLE= 0;
      DEBUG && printf("THREAD %ld WAKE UP\n", thread->id);
      //lo soltamos
      pthread_mutex_unlock(&thread->processToThreadQueue.queueLock);
      DEBUG && printf("THREAD %ld SUELTA processToThreadQueue_mutex\n", thread->id);
      
    }

  }
  
  thread->context.Dispose();
  DEBUG && printf("THREAD %ld EVENTLOOP EXIT\n", thread->id);
}






static void cleanUpAfterThread (typeThread* thread) {
  
  DEBUG && printf("DESTROYATHREAD %ld IN MAIN THREAD #1\n", thread->id);
  
  //TODO: hay que vaciar las colas y destruir los trabajos antes de ponerlas a NULL
  thread->processToThreadQueue.first= thread->processToThreadQueue.last= NULL;
  thread->threadToProcessQueue.first= thread->threadToProcessQueue.last= NULL;
  thread->JSObject->SetPointerInInternalField(0, NULL);
  thread->JSObject.Dispose();
  
  DEBUG && printf("DESTROYATHREAD %ld IN MAIN THREAD #2\n", thread->id);
  
#ifdef TAGG_USE_LIBUV
  uv_close((uv_handle_t*) &thread->async_watcher, NULL);
  //uv_unref(&thread->async_watcher);
#else
  ev_async_stop(EV_DEFAULT_UC_ &thread->async_watcher);
  ev_unref(EV_DEFAULT_UC);
#endif
  
  DEBUG && printf("DESTROYATHREAD %ld IN MAIN THREAD #FINAL\n", thread->id);
  
  //free(thread);
}






// C callback that runs in node's main thread. This is called by node's event loop
// when the thread tells it to do so. This is the one responsible for
// calling the thread's JS callback in node's js context in node's main thread.
static void Callback (
#ifdef TAGG_USE_LIBUV
  uv_async_t *watcher
#else
  EV_P_ ev_async *watcher
#endif
                           , int revents) {
  typeThread* thread= (typeThread*) watcher;
  
  if (thread->ended) {
    cleanUpAfterThread(thread);
    return;
  }
  
  HandleScope scope;
  typeJob* job;
  Local<Value> argv[2];
  Local<Value> null= Local<Value>::New(Null());
  typeQueueItem* qitem;
  String::Utf8Value* str;
  
  TryCatch onError;
  while ((qitem= qPull(&thread->threadToProcessQueue))) {
  
    DEBUG && printf("CALLBACK %ld IN MAIN THREAD\n", thread->id);
    
    job= &qitem->job;

    if (job->jobType == kJobTypeEval) {

      if (job->eval.tiene_callBack) {
        str= job->eval.resultado;

        if (job->eval.error) {
          argv[0]= Exception::Error(String::New(**str, (*str).length()));
          argv[1]= null;
        } else {
          argv[0]= null;
          argv[1]= String::New(**str, (*str).length());
        }
        job->cb->CallAsFunction(thread->JSObject, 2, argv);
        //job->cb.Dispose();
        job->eval.tiene_callBack= 0;

        delete str;
        job->eval.resultado= NULL;
      }

      qPush(qitem, freeJobsQueue);
      
      if (onError.HasCaught()) {
        if (thread->threadToProcessQueue.first) {
          WAKEUP_EVENT_LOOP
        }
        node::FatalException(onError);
        return;
      }
    }
    else if (job->jobType == kJobTypeEvent) {
      
      //fprintf(stdout, "*** Callback\n");
      
      Local<Value> args[2];
      
      str= job->event.eventName;
      args[0]= String::New(**str, (*str).length());
      delete str;
      
      Local<Array> array= Array::New(job->event.length);
      args[1]= array;
      
      int i= 0;
      while (i < job->event.length) {
        str= job->event.argumentos[i];
        array->Set(i, String::New(**str, (*str).length()));
        delete str;
        i++;
      }
      
      free(job->event.argumentos);
      qPush(qitem, freeJobsQueue);
      thread->dispatchEvents->CallAsFunction(thread->JSObject, 2, args);
    }
  }
}






// Tell a thread to quit, either nicely or rudely.
static Handle<Value> Destroy (const Arguments &args) {

  //thread.destroy() or thread.destroy(0) means nicely (the deafult)
  //thread destroy(1) means rudely.
  //When done nicely the thread will quit only if/when there aren't any jobs pending
  //in its jobsQueue nor nextTick()ed functions to execute in the nextTick queue _ntq[]
  //When done rudely it will try to exit the event loop regardless.
  //If the thread is stuck in a ` while (1) ; ` or something this won't work...
  
  HandleScope scope;
  //TODO: Hay que comprobar que this en un objeto y que tiene hiddenRefTotypeThread_symbol y que no es nil
  //TODO: Aquí habría que usar static void TerminateExecution(int thread_id);
  //TODO: static void v8::V8::TerminateExecution  ( Isolate *   isolate= NULL   )   [static]
  
  int arg= 0;
  typeThread* thread= isAThread(args.This());
  if (!thread) {
    return ThrowException(Exception::TypeError(String::New("thread.destroy(): the receiver must be a thread object")));
  }
  
  if (args.Length()) {
    arg= args[0]->ToNumber()->NumberValue();
  }

  DEBUG && printf("DESTROY(%d) THREAD %ld \n", arg, thread->id);

  DEBUG && printf("KILLING [%d] THREAD %ld #1\n", arg, thread->id);
  //pthread_cancel(thread->thread);
  pthread_mutex_lock(&thread->processToThreadQueue.queueLock);
  DEBUG && printf("KILLING [%d] THREAD %ld #2\n", arg, thread->id);
  thread->sigkillNicely= !arg;
  thread->sigkill= arg;
  if (thread->IDLE) {
    DEBUG && printf("KILLING [%d] THREAD %ld #3\n", arg, thread->id);
    pthread_cond_signal(&thread->IDLE_cv);
  }
  DEBUG && printf("KILLING [%d] THREAD %ld #4\n", arg, thread->id);
  pthread_mutex_unlock(&thread->processToThreadQueue.queueLock);
  DEBUG && printf("KILLING [%d] THREAD %ld #5\n", arg, thread->id);

  return Undefined();
}






// Eval: Pushes a job into the thread's ->processToThreadQueue.
static Handle<Value> Eval (const Arguments &args) {
  HandleScope scope;
  
  if (!args.Length()) {
    return ThrowException(Exception::TypeError(String::New("thread.eval(program [,callback]): missing arguments")));
  }
  
  typeThread* thread= isAThread(args.This());
  if (!thread) {
    return ThrowException(Exception::TypeError(String::New("thread.eval(): the receiver must be a thread object")));
  }

  typeQueueItem* qitem= nuJobQueueItem();
  typeJob* job= &qitem->job;
  
  job->eval.tiene_callBack= ((args.Length() > 1) && (args[1]->IsFunction()));
  if (job->eval.tiene_callBack) {
    job->cb= Persistent<Object>::New(args[1]->ToObject());
  }
  job->eval.scriptText_StringObject= new String::Utf8Value(args[0]);
  job->eval.useStringObject= 1;
  job->jobType= kJobTypeEval;
  
  pushJobToThread(qitem, thread);
  return scope.Close(args.This());
}





static char* readFile (Handle<String> path) {
  v8::String::Utf8Value c_str(path);
  FILE* fp= fopen(*c_str, "rb");
  if (!fp) {
    fprintf(stderr, "Error opening the file %s\n", *c_str);
    //@bruno: Shouldn't we throw, here ?
    return NULL;
  }
  fseek(fp, 0, SEEK_END);
  long len= ftell(fp);
  rewind(fp); //fseek(fp, 0, SEEK_SET);
  char *buf= (char*) calloc(len + 1, sizeof(char)); // +1 to get null terminated string
  fread(buf, len, 1, fp);
  fclose(fp);
  /*
  DEBUG && printf("SOURCE:\n%s\n", buf);
  fflush(stdout);
  */
  return buf;
}






// Load: Loads from file and passes to Eval
static Handle<Value> Load (const Arguments &args) {
  HandleScope scope;

  if (!args.Length()) {
    return ThrowException(Exception::TypeError(String::New("thread.load(filename [,callback]): missing arguments")));
  }

  typeThread* thread= isAThread(args.This());
  if (!thread) {
    return ThrowException(Exception::TypeError(String::New("thread.load(): the receiver must be a thread object")));
  }
  
  char* source= readFile(args[0]->ToString());  //@Bruno: here we don't know if the file was not found or if it was an empty file
  if (!source) return scope.Close(args.This()); //@Bruno: even if source is empty, we should call the callback ?

  typeQueueItem* qitem= nuJobQueueItem();
  typeJob* job= &qitem->job;

  job->eval.tiene_callBack= ((args.Length() > 1) && (args[1]->IsFunction()));
  if (job->eval.tiene_callBack) {
    job->cb= Persistent<Object>::New(args[1]->ToObject());
  }
  job->eval.scriptText_CharPtr= source;
  job->eval.useStringObject= 0;
  job->jobType= kJobTypeEval;

  pushJobToThread(qitem, thread);
  return scope.Close(args.This());
}






static Handle<Value> processEmit (const Arguments &args) {
  HandleScope scope;
  
  if (!args.Length()) return scope.Close(args.This());
  
  typeThread* thread= isAThread(args.This());
  if (!thread) {
    return ThrowException(Exception::TypeError(String::New("thread.emit(): the receiver must be a thread object")));
  }
  
  typeQueueItem* qitem= nuJobQueueItem();
  typeJob* job= &qitem->job;
  
  job->jobType= kJobTypeEvent;
  job->event.length= args.Length()- 1;
  job->event.eventName= new String::Utf8Value(args[0]);
  job->event.argumentos= (v8::String::Utf8Value**) malloc(job->event.length* sizeof(void*));
  
  int i= 1;
  do {
    job->event.argumentos[i-1]= new String::Utf8Value(args[i]);
  } while (++i <= job->event.length);
  
  pushJobToThread(qitem, thread);
  return scope.Close(args.This());
}






static Handle<Value> threadEmit (const Arguments &args) {
  HandleScope scope;
  
  if (!args.Length()) return scope.Close(args.This());
  
  typeThread* thread= (typeThread*) Isolate::GetCurrent()->GetData();
  typeQueueItem* qitem= nuJobQueueItem();
  typeJob* job= &qitem->job;
  
  job->jobType= kJobTypeEvent;
  job->event.length= args.Length()- 1;
  job->event.eventName= new String::Utf8Value(args[0]);
  job->event.argumentos= (v8::String::Utf8Value**) malloc(job->event.length* sizeof(void*));
  
  int i= 1;
  do {
    job->event.argumentos[i-1]= new String::Utf8Value(args[i]);
  } while (++i <= job->event.length);
  
  qPush(qitem, &thread->threadToProcessQueue);
  WAKEUP_EVENT_LOOP
  return scope.Close(args.This());
}








// Creates and launches a new isolate in a new background thread.
static Handle<Value> Create (const Arguments &args) {
    HandleScope scope;
    
    typeThread* thread= (typeThread*) calloc(1, sizeof (typeThread));
    thread->id= threadsCtr++;
    thread->threadMagicCookie= kThreadMagicCookie;
    thread->JSObject= Persistent<Object>::New(threadTemplate->NewInstance());
    thread->JSObject->SetPointerInInternalField(0, thread);
    thread->JSObject->Set(id_symbol, Integer::New(thread->id));
    Local<Value> dispatchEvents= Script::Compile(String::New(kEvents_js))->Run()->ToObject()->CallAsFunction(thread->JSObject, 0, NULL);
    thread->dispatchEvents= Persistent<Object>::New(dispatchEvents->ToObject());
    
#ifdef TAGG_USE_LIBUV
    uv_async_init(uv_default_loop(), &thread->async_watcher, Callback);
#else
    ev_async_init(&thread->async_watcher, Callback);
    ev_async_start(EV_DEFAULT_UC_ &thread->async_watcher);
    ev_ref(EV_DEFAULT_UC);
#endif
    
    pthread_cond_init(&(thread->IDLE_cv), NULL);
    pthread_mutex_init(&(thread->processToThreadQueue.queueLock), NULL);
    pthread_mutex_init(&(thread->threadToProcessQueue.queueLock), NULL);
    if (pthread_create(&(thread->thread), NULL, threadBootProc, thread)) {
      //Algo ha ido mal, toca deshacer todo
      pthread_cond_destroy(&(thread->IDLE_cv));
      pthread_mutex_destroy(&(thread->processToThreadQueue.queueLock));
      pthread_mutex_destroy(&(thread->threadToProcessQueue.queueLock));
      
#ifdef TAGG_USE_LIBUV
      uv_close((uv_handle_t*) &thread->async_watcher, NULL);
      //uv_unref(&thread->async_watcher);
#else
      ev_async_stop(EV_DEFAULT_UC_ &thread->async_watcher);
      ev_unref(EV_DEFAULT_UC);
#endif

      thread->JSObject.Dispose();
      free(thread);
      return ThrowException(Exception::TypeError(String::New("create(): error in pthread_create()")));
    }

    return scope.Close(thread->JSObject);
}


void Init (Handle<Object> target) {
  
  freeJobsQueue= nuQueue();
  HandleScope scope;
  useLocker= v8::Locker::IsActive();
  id_symbol= Persistent<String>::New(String::NewSymbol("id"));
  
  target->Set(String::NewSymbol("create"), FunctionTemplate::New(Create)->GetFunction());
  target->Set(String::NewSymbol("createPool"), Script::Compile(String::New(kCreatePool_js))->Run()->ToObject());
  
  threadTemplate= Persistent<ObjectTemplate>::New(ObjectTemplate::New());
  threadTemplate->SetInternalFieldCount(1);
  threadTemplate->Set(id_symbol, Integer::New(0));
  threadTemplate->Set(String::NewSymbol("eval"), FunctionTemplate::New(Eval));
  threadTemplate->Set(String::NewSymbol("load"), FunctionTemplate::New(Load));
  threadTemplate->Set(String::NewSymbol("emit"), FunctionTemplate::New(processEmit));
  threadTemplate->Set(String::NewSymbol("destroy"), FunctionTemplate::New(Destroy));
  
}







NODE_MODULE(threads_a_gogo, Init)

/*
gcc -E -I /Users/jorge/JAVASCRIPT/binarios/include/node -o /o.c /Users/jorge/JAVASCRIPT/threads_a_gogo/src/threads_a_gogo.cc && mate /o.c
*/