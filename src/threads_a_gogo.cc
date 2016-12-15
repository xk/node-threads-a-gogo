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
  int done;
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
  typeQueueItem* pushPtr;
  typeQueueItem* pullPtr;
} typeQueue;

typedef enum killTypes {
  kKillNicely= 1,
  kKillRudely
} killTypes;

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
  
  typeQueue* processToThreadQueue;  //Jobs to run in the thread
  typeQueue* threadToProcessQueue;  //Jobs to run in node's main thread
  
  volatile int IDLE;
  pthread_cond_t idle_cv;
  pthread_mutex_t idle_mutex;
  
  Isolate* isolate;
  Persistent<Context> context;
  Persistent<Object> nodeJSObject;
  Persistent<Object> nodeDispatchEvents;
  int hasDestroyCallback;
  Persistent<Object> destroyCallback;
  
  unsigned long threadMagicCookie;
} typeThread;



static inline void qPush (typeQueueItem* qitem, typeQueue* queue);
static inline typeQueueItem* qPull (typeQueue* queue);
static inline typeQueueItem* nuQitem ();
static typeQueue* nuQueue ();
static void destroyQueue (typeQueue* q);
static typeThread* isAThread (Handle<Object> receiver);
static void pushJobToThread (typeQueueItem* qitem, typeThread* thread);
static Handle<Value> Puts (const Arguments &args);
static void* threadBootProc (void* arg);
static void eventLoop (typeThread* thread);
static void cleanUpAfterThread (typeThread* thread);
static void Callback (
#ifdef TAGG_USE_LIBUV
  uv_async_t *watcher
#else
  EV_P_ ev_async *watcher
#endif
                           , int status);
static Handle<Value> Destroy (const Arguments &args);
static Handle<Value> Eval (const Arguments &args);
static Handle<Value> processEmit (const Arguments &args);
static Handle<Value> threadEmit (const Arguments &args);
static Handle<Value> Create (const Arguments &args);
void Init (Handle<Object> target);


static bool useLocker;
static long int threadsCtr= 0;
static Persistent<String> id_symbol;
static Persistent<String> load_symbol;
static Persistent<Object> load_js;
static Persistent<ObjectTemplate> threadTemplate;




/*

cd deps/minifier/src
gcc minify.c -o minify
cat ../../../src/events.js | ./minify kEvents_js > ../../../src/kEvents_js
cat ../../../src/load.js | ./minify kLoad_js > ../../../src/kLoad_js
cat ../../../src/createPool.js | ./minify kCreatePool_js > ../../../src/kCreatePool_js
cat ../../../src/nextTick.js | ./minify kNextTick_js > ../../../src/kNextTick_js

*/

#include "load.js.c"
#include "events.js.c"
#include "nextTick.js.c"
#include "createPool.js.c"

//node-waf configure uninstall distclean configure build install




static inline void qPush (typeQueueItem* qitem, typeQueue* queue) {
  qitem->next= NULL;
  queue->pushPtr->next= qitem;
  queue->pushPtr= qitem;
}

static inline typeQueueItem* qPull (typeQueue* queue) {
  typeQueueItem* qitem= queue->pullPtr;
  while (qitem->job.done && qitem->next) {
    queue->pullPtr= qitem->next;
    free(qitem);
    qitem= queue->pullPtr;
  }
  return qitem->job.done ? NULL : qitem;
}

static inline typeQueueItem* nuQitem () {
  typeQueueItem* qitem= (typeQueueItem*) calloc(1, sizeof(typeQueueItem));
  return qitem;
}

static typeQueue* nuQueue () {
  typeQueue* queue= (typeQueue*) calloc(1, sizeof(typeQueue));
  typeQueueItem* qitem= nuQitem();
  qitem->job.done= 1;
  queue->pullPtr= queue->pushPtr= qitem;
  return queue;
}

static void destroyQueue (typeQueue* queue) {
  typeQueueItem* qitem= queue->pullPtr;
  while (qitem) {
    queue->pullPtr= qitem->next;
    free(qitem);
    qitem= queue->pullPtr;
  }
  free(queue);
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

  DEBUG && printf("THREAD %ld PUSH JOB TO THREAD #1\n", thread->id);
  //Esto garantiza que haya algo en la queue porque si no
  //la thread podría echarse a dormir (tb podría ya estar durmiendo)
  //si ve que no hay nada en la queue
  qPush(qitem, thread->processToThreadQueue);
  
  //Cogiendo este lock sabemos que la thread o no ha salido aún
  //del event loop o está parada en wait/sleep/idle
  pthread_mutex_lock(&thread->idle_mutex);
  
  DEBUG && printf("THREAD %ld PUSH JOB TO THREAD #2\n", thread->id);
  //Estamos seguros de que no se está tocando thread->IDLE
  //xq tenemos el lock nosotros y sólo se toca con el lock puesto
  if (thread->IDLE) {
    //estaba parada, hay que ponerla en marcha
    DEBUG && printf("THREAD %ld PUSH JOB TO THREAD #3\n", thread->id);
    pthread_cond_signal(&thread->idle_cv);
  }
  //Hay que volver a soltar el lock
  pthread_mutex_unlock(&thread->idle_mutex);
  DEBUG && printf("THREAD %ld PUSH JOB TO THREAD #5 EXIT\n", thread->id);
}






static Handle<Value> Puts (const Arguments &args) {
  int i= 0;
  while (i < args.Length()) {
    HandleScope scope;
    String::Utf8Value c_str(args[i]);
    fputs(*c_str, stdout);
    i++;
  }
  fflush(stdout);
  return Undefined();
}












static void* threadBootProc (void* arg) {

//Esto es lo primero que se ejecuta en la(s) thread(s) al nacer.
//Básicamente inicializa lo necesario y se entra en el eventloop

  int dummy;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &dummy);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &dummy);
  
  typeThread* thread= (typeThread*) arg;
  
  DEBUG && printf("THREAD %ld BOOT ENTER\n", thread->id);
  
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
  DEBUG && printf("THREAD %ld BOOT EXIT #4 WAKEUP_EVENT_LOOP\n", thread->id);
  WAKEUP_EVENT_LOOP
  DEBUG && printf("THREAD %ld BOOT EXIT #5 ENDED\n", thread->id);
  return 0;
}











static void eventLoop (typeThread* thread) {

// The thread's eventloop runs in the thread(s) not in node's main thread

  DEBUG && printf("THREAD %ld EVENTLOOP ENTER\n", thread->id);

  thread->isolate->Enter();
  thread->context= Context::New();
  thread->context->Enter();
  
  {
    HandleScope scope1;
    Persistent<String> _ntq= Persistent<String>::New(String::NewSymbol("_ntq"));
    Local<Object> global= thread->context->Global();
    global->Set(String::NewSymbol("puts"), FunctionTemplate::New(Puts)->GetFunction());
    Local<Object> threadObject= Object::New();
    global->Set(String::NewSymbol("thread"), threadObject);
    threadObject->Set(String::NewSymbol("id"), Number::New(thread->id));
    threadObject->Set(String::NewSymbol("emit"), FunctionTemplate::New(threadEmit)->GetFunction());
    Local<Object> threadDispatchEvents= Script::Compile(String::New(kEvents_js))->Run()->ToObject()->CallAsFunction(threadObject, 0, NULL)->ToObject();
    Local<Object> dispatchNextTicks= Script::Compile(String::New(kNextTick_js))->Run()->ToObject()->CallAsFunction(threadObject, 0, NULL)->ToObject();
    
    //SetFatalErrorHandler(FatalErrorCB);
    
    typeJob* job;
    typeQueueItem *qitem= NULL;
    typeQueueItem *qitem2, *qitem3;
    while (1) {
      
      {
        HandleScope scope2;
        TryCatch onError;
        String::Utf8Value* str;
        Local<String> source;
        Local<Script> script;
        Local<Value> resultado;
        double ntql;
        
        DEBUG && printf("THREAD %ld BEFORE WHILE\n", thread->id);
        
        while (1) {
          
          DEBUG && printf("THREAD %ld WHILE\n", thread->id);
          
          if (thread->sigkill == kKillRudely) break;
          
          if (qitem || (qitem= qPull(thread->processToThreadQueue))) {
            HandleScope scope;
            
            qitem2= qitem;
            qitem= NULL;
            DEBUG && printf("THREAD %ld QITEM\n", thread->id);
            job= &qitem2->job;
            if (job->jobType == kJobTypeEval) {
              //Ejecutar un texto
              HandleScope scope;
              
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
                qitem3= nuQitem();
                memcpy(qitem3, qitem2, sizeof(typeQueueItem));
                qitem3->job.eval.error= onError.HasCaught() ? 1 : 0;
                qitem3->job.eval.resultado= new String::Utf8Value(job->eval.error ? onError.Exception() : resultado);
                qPush(qitem3, thread->threadToProcessQueue);
                WAKEUP_EVENT_LOOP
              }
              
              if (onError.HasCaught()) onError.Reset();
              
              job->done= 1;
            }
            else if (job->jobType == kJobTypeEvent) {
              HandleScope scope;
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
              threadDispatchEvents->CallAsFunction(global, 2, args);
              job->done= 1;
            }
          }
          else
            DEBUG && printf("THREAD %ld NO QITEM\n", thread->id);

          if (thread->sigkill == kKillRudely) break;
          
          {
            HandleScope scope;
            DEBUG && printf("THREAD %ld NTQL\n", thread->id);
            ntql= dispatchNextTicks->CallAsFunction(threadObject, 0, NULL)->ToNumber()->Value();
            if (onError.HasCaught()) onError.Reset();
          }
          
          if (!ntql && !(qitem || (qitem= qPull(thread->processToThreadQueue)))) {
            DEBUG && printf("THREAD %ld EXIT WHILE: NO NTQL AND NO QITEM\n", thread->id);
            break;
          }
          
        }
        
      }
      
      if (thread->sigkill) break;

      V8::IdleNotification();
      
      DEBUG && printf("THREAD %ld BEFORE MUTEX\n", thread->id);
      //cogemos el lock para
      //por un lado poder mirar si hay cosas en la queue sabiendo
      //que nadie la está tocando
      //y por otro lado para poder tocar thread->IDLE sabiendo
      //que nadie la está mirando mientras la tocamos.
      pthread_mutex_lock(&thread->idle_mutex);
      DEBUG && printf("THREAD %ld TIENE processToThreadQueue_MUTEX\n", thread->id);
      //aquí tenemos acceso exclusivo a processToThreadQueue y a thread->IDLE
      while (!(qitem || (qitem= qPull(thread->processToThreadQueue))) && !thread->sigkill) {
        //sólo se entra aquí si no hay nada en la queue y no hay sigkill
        //hemos avisado con thread->IDLE de que nos quedamos parados
        // para que sepan que nos han de despertar
        thread->IDLE= 1;
        DEBUG && printf("THREAD %ld SLEEP\n", thread->id);
        //en pthread_cond_wait se quedará atascada esta thread hasta que
        //nos despierten y haya cosas en la queue o haya sigkill
        //El lock se abre al entrar en pthread_cond_wait así que los
        //demás ahora van a poder mirar thread->IDLE mientras estamos parados/durmiendo
        pthread_cond_wait(&thread->idle_cv, &thread->idle_mutex);
        //El lock queda cerrado al salir de pthread_cond_wait pero no importa xq
        //si seguimos en el bucle se va a volver a abrir y si salimos tb
      }
      //Aquí aún tenemos el lock así que podemos tocar thread->IDLE con seguridad
      thread->IDLE= 0;
      DEBUG && printf("THREAD %ld WAKE UP\n", thread->id);
      //lo soltamos
      pthread_mutex_unlock(&thread->idle_mutex);
      DEBUG && printf("THREAD %ld SUELTA processToThreadQueue_mutex\n", thread->id);
      
    }

  }
  
  thread->context.Dispose();
  DEBUG && printf("THREAD %ld EVENTLOOP EXIT\n", thread->id);
}



static void cleanUpAfterThreadCallback (uv_handle_t* arg) {
  HandleScope scope;
  typeThread* thread= (typeThread*) arg;
  DEBUG && printf("THREAD %ld cleanUpAfterThreadCallback()\n", thread->id);
  if (thread->hasDestroyCallback) {
    thread->destroyCallback->CallAsFunction(Context::GetCurrent()->Global(), 0, NULL);
  } 
  free(thread);
}


static void cleanUpAfterThread (typeThread* thread) {
  
  DEBUG && printf("THREAD %ld cleanUpAfterThread() IN MAIN THREAD #1\n", thread->id);
  
  //(*TO_DO*): hay que vaciar las colas y destruir los trabajos y sus objetos antes de ponerlas a NULL
  
  destroyQueue(thread->processToThreadQueue);
  destroyQueue(thread->threadToProcessQueue);
  
  pthread_cond_destroy(&(thread->idle_cv));
  pthread_mutex_destroy(&(thread->idle_mutex));
  thread->nodeDispatchEvents.Dispose();
  thread->nodeJSObject.Dispose();
  
  if (thread->ended) {
    // Esta thread llegó a funcionar alguna vez
    // hay que apagar uv antes de poder hacer free(thread)
    // De hecho el free(thread) se hará en una Callabck xq uv_close la va a llamar
    
    DEBUG && printf("THREAD %ld cleanUpAfterThread() FREE IN UV CALLBACK #2\n", thread->id);
    
#ifdef TAGG_USE_LIBUV
    uv_close((uv_handle_t*) &thread->async_watcher, cleanUpAfterThreadCallback);
    //uv_unref(&thread->async_watcher);
#else
    ev_async_stop(EV_DEFAULT_UC_ &thread->async_watcher);
    ev_unref(EV_DEFAULT_UC);
#endif

  }
  else {
    //Esta thread nunca ha llegado a arrancar
    //Seguramente venimos de un error en thread.create())
    DEBUG && printf("THREAD %ld cleanUpAfterThread() FREE HERE #3\n", thread->id);
    free(thread);
  }
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
                           , int status) {
                           
  HandleScope scope;
  
  typeJob* job;
  typeQueueItem* qitem;
  typeThread* thread= (typeThread*) watcher;
  
  if (thread->ended) {
    DEBUG && printf("THREAD %ld CALLBACK CALLED cleanUpAfterThread()\n", thread->id);
    //pthread_cancel(thread->thread);
    cleanUpAfterThread(thread);
    return;
  }
  
  Local<Array> array;
  Local<Value> args[2];
  String::Utf8Value* str;
  Local<Value> null= Local<Value>::New(Null());
  
  TryCatch onError;
  while ((qitem= qPull(thread->threadToProcessQueue))) {
  
    DEBUG && printf("CALLBACK %ld IN MAIN THREAD\n", thread->id);
    
    job= &qitem->job;

    if (job->jobType == kJobTypeEval) {

      if (job->eval.tiene_callBack) {
      
        str= job->eval.resultado;

        if (job->eval.error) {
          args[0]= Exception::Error(String::New(**str, (*str).length()));
          args[1]= null;
        } else {
          args[0]= null;
          args[1]= String::New(**str, (*str).length());
        }
        job->cb->CallAsFunction(thread->nodeJSObject, 2, args);
        job->cb.Dispose();
        job->eval.tiene_callBack= 0;
        delete job->eval.resultado;
      }

      job->done= 1;
      
      if (onError.HasCaught()) {
        WAKEUP_EVENT_LOOP
        node::FatalException(onError);
        return;
      }
    }
    else if (job->jobType == kJobTypeEvent) {
      
      //fprintf(stdout, "*** Callback\n");
      
      str= job->event.eventName;
      args[0]= String::New(**str, (*str).length());
      delete job->event.eventName;
      array= Array::New(job->event.length);
      args[1]= array;
      
      if (job->event.length) {
        int i= 0;
        do {
          str= job->event.argumentos[i];
          array->Set(i, String::New(**str, (*str).length()));
          delete job->event.argumentos[i];
        } while (++i < job->event.length);
        free(job->event.argumentos);
      }

      thread->nodeDispatchEvents->CallAsFunction(Context::GetCurrent()->Global(), 2, args);
      job->done= 1;
    }
  }
}






// Tell a thread to quit, either nicely or rudely.
static Handle<Value> Destroy (const Arguments &args) {

  //thread.destroy() or thread.destroy(0) means nicely (the default)
  //thread destroy(1) means rudely.
  //When done nicely the thread will quit only if/when there aren't anymore jobs pending
  //in its jobsQueue nor nextTick()ed functions to execute in the nextTick queue _ntq[]
  //When done rudely it will try to exit the event loop regardless.
  //If the thread is stuck in a ` while (1) ; ` or something this won't work... (*TO_DO*)
  
  HandleScope scope;
  //TODO: Hay que comprobar que this en un objeto y que tiene hiddenRefTotypeThread_symbol y que no es nil
  //TODO: Aquí habría que usar static void TerminateExecution(int thread_id);
  //TODO: static void v8::V8::TerminateExecution  ( Isolate *   isolate= NULL   )   [static]
  
  typeThread* thread= isAThread(args.This());
  if (!thread) {
    return ThrowException(Exception::TypeError(String::New("thread.destroy(): the receiver must be a thread object")));
  }
  
  int arg= kKillNicely;
  if (args.Length()) {
    arg= args[0]->ToNumber()->Value() ? kKillRudely : kKillNicely;
  }
  
  thread->hasDestroyCallback= (args.Length() > 1) && (args[1]->IsFunction());
  if (thread->hasDestroyCallback) {
    thread->destroyCallback= Persistent<Object>::New(args[1]->ToObject());
  }
  
  const char* str= arg == kKillNicely ? "NICELY" : "RUDELY";
  DEBUG && printf("THREAD %ld DESTROY(%s) #1\n", thread->id, str);
  pthread_mutex_lock(&thread->idle_mutex);
  DEBUG && printf("THREAD %ld DESTROY(%s) #2\n", thread->id, str);
  thread->sigkill= arg;
  if (thread->IDLE) {
    DEBUG && printf("THREAD %ld DESTROY(%s) #3\n", thread->id, str);
    pthread_cond_signal(&thread->idle_cv);
  }
  pthread_mutex_unlock(&thread->idle_mutex);
  DEBUG && printf("THREAD %ld DESTROY(%s) #4 EXIT\n", thread->id, str);

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

  typeQueueItem* qitem= nuQitem();
  typeJob* job= &qitem->job;
  
  job->eval.tiene_callBack= ((args.Length() > 1) && (args[1]->IsFunction()));
  if (job->eval.tiene_callBack) {
    job->cb= Persistent<Object>::New(args[1]->ToObject());
  }
  job->eval.scriptText_StringObject= new String::Utf8Value(args[0]);
  job->eval.useStringObject= 1;
  job->jobType= kJobTypeEval;
  
  pushJobToThread(qitem, thread);
  return args.This();
}












static Handle<Value> processEmit (const Arguments &args) {
  HandleScope scope;
  
  if (!args.Length()) return args.This();
  
  typeThread* thread= isAThread(args.This());
  if (!thread) {
    return ThrowException(Exception::TypeError(String::New("thread.emit(): the receiver must be a thread object")));
  }
  
  typeQueueItem* qitem= nuQitem();
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
  return args.This();
}






static Handle<Value> threadEmit (const Arguments &args) {
  HandleScope scope;
  
  if (!args.Length()) return args.This();
  
  typeThread* thread= (typeThread*) Isolate::GetCurrent()->GetData();
  typeQueueItem* qitem= nuQitem();
  typeJob* job= &qitem->job;
  
  job->jobType= kJobTypeEvent;
  job->event.eventName= new String::Utf8Value(args[0]);
  if (args.Length() > 1) {
    job->event.length= args.Length()- 1;
    job->event.argumentos= (String::Utf8Value**) malloc(job->event.length * sizeof(void*));
    int i= 0;
    do {
      job->event.argumentos[i]= new String::Utf8Value(args[i]);
    } while (++i < job->event.length);
  }
  
  qPush(qitem, thread->threadToProcessQueue);
  WAKEUP_EVENT_LOOP
  return args.This();
}








// Creates and launches a new isolate in a new background thread.
static Handle<Value> Create (const Arguments &args) {
    HandleScope scope;
    
    typeThread* thread= (typeThread*) calloc(1, sizeof (typeThread));
    thread->id= threadsCtr++;
    thread->threadMagicCookie= kThreadMagicCookie;
    thread->processToThreadQueue= nuQueue();
    thread->threadToProcessQueue= nuQueue();
    thread->nodeJSObject= Persistent<Object>::New(threadTemplate->NewInstance());
    thread->nodeJSObject->SetPointerInInternalField(0, thread);
    thread->nodeJSObject->Set(id_symbol, Integer::New(thread->id));
    Local<Value> dispatchEvents= Script::Compile(String::New(kEvents_js))->Run()->ToObject()->CallAsFunction(thread->nodeJSObject, 0, NULL);
    thread->nodeDispatchEvents= Persistent<Object>::New(dispatchEvents->ToObject());
    
    printf("LO INTENTO PERO NO VA\n"), fflush(stdout);
    thread->nodeJSObject->Set(load_symbol, load_js);
    
    pthread_cond_init(&(thread->idle_cv), NULL);
    pthread_mutex_init(&(thread->idle_mutex), NULL);
    
    char* errstr;
    int err, retry= 5;
    do {
      err= pthread_create(&(thread->thread), NULL, threadBootProc, thread);
      //pthread_detach(pthread_t thread); ???
      if (!err) break;
      errstr= strerror(err);
      printf("THREAD %ld PTHREAD_CREATE() ERROR %d : %s RETRYING %d\n", thread->id, err, errstr, retry);
      usleep(50000);
    } while (--retry);
    
    if (err) {
      //Algo ha ido mal, toca deshacer todo
      printf("THREAD %ld PTHREAD_CREATE() ERROR %d : %s NOT RETRYING ANY MORE\n", thread->id, err, errstr);
      DEBUG && printf("CALLED cleanUpAfterThread %ld FROM CREATE()\n", thread->id);
      cleanUpAfterThread(thread);
      return ThrowException(Exception::TypeError(String::New("create(): error in pthread_create()")));
    }
    else {
    
#ifdef TAGG_USE_LIBUV
      uv_async_init(uv_default_loop(), &thread->async_watcher, Callback);
#else
      ev_async_init(&thread->async_watcher, Callback);
      ev_async_start(EV_DEFAULT_UC_ &thread->async_watcher);
      ev_ref(EV_DEFAULT_UC);
#endif
    
    }

    return thread->nodeJSObject;
}


void Init (Handle<Object> target) {

  HandleScope scope;
  useLocker= v8::Locker::IsActive();
  id_symbol= Persistent<String>::New(String::NewSymbol("id"));
  load_symbol= Persistent<String>::New(String::NewSymbol("load"));
  load_js= Persistent<Object>::New(Script::Compile(String::New(kLoad_js))->Run()->ToObject());
  
  target->Set(String::NewSymbol("create"), FunctionTemplate::New(Create)->GetFunction());
  target->Set(String::NewSymbol("createPool"), Script::Compile(String::New(kCreatePool_js))->Run()->ToObject());
  
  threadTemplate= Persistent<ObjectTemplate>::New(ObjectTemplate::New());
  threadTemplate->SetInternalFieldCount(1);
  threadTemplate->Set(id_symbol, Integer::New(0));
  threadTemplate->Set(String::NewSymbol("eval"), FunctionTemplate::New(Eval));
  threadTemplate->Set(String::NewSymbol("emit"), FunctionTemplate::New(processEmit));
  threadTemplate->Set(String::NewSymbol("destroy"), FunctionTemplate::New(Destroy));
  
}


NODE_MODULE(threads_a_gogo, Init)

/*
gcc -E -I /Users/jorge/JAVASCRIPT/binarios/include/node -o /o.c /Users/jorge/JAVASCRIPT/threads_a_gogo/src/threads_a_gogo.cc && mate /o.c
*/