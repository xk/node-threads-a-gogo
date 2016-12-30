//2011-11, 2016-12 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//threads_a_gogo.cc

#include <node.h>
#include <v8.h>
#include <uv.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <node_version.h>

/*
//using namespace node;
//using namespace v8;
*/

//Macros BEGIN

#define kThreadMagicCookie 0x99c0ffee

#define TAGG_USE_LIBUV
#if (NODE_MAJOR_VERSION == 0) && (NODE_MINOR_VERSION <= 5)
  #undef TAGG_USE_LIBUV
#endif

#ifdef TAGG_USE_LIBUV
  #define WAKEUP_NODE_EVENT_LOOP uv_async_send(&thread->async_watcher);
#else
  #define WAKEUP_NODE_EVENT_LOOP ev_async_send(EV_DEFAULT_UC_ &thread->async_watcher);
#endif

#define TAGG_USE_NEW_API
#if (NODE_MAJOR_VERSION == 0) && (NODE_MINOR_VERSION < 12)
  #undef TAGG_USE_NEW_API
#endif

#ifdef TAGG_USE_NEW_API
  #define TAGG_USE_ALLOCATOR
  #if (NODE_MAJOR_VERSION < 3)
    #undef TAGG_USE_ALLOCATOR
  #endif
#endif

#ifdef TAGG_USE_ALLOCATOR
  #define TAGG_USE_DEFAULT_ALLOCATOR
  #if (NODE_MAJOR_VERSION < 7)
    #undef TAGG_USE_DEFAULT_ALLOCATOR
  #endif
#endif

//Macros END

//Type definitions BEGIN

typedef enum eventTypes {
  kEventTypeEmpty = 0,
  kEventTypeEmit,
  kEventTypeEval,
  kEventTypeLoad
} eventTypes;

struct emitStruct {
  int argc;
  char** argv;
  char* eventName;
};

struct evalStruct {
  int error;
  int hasCallback;
  union {
    char* resultado;
    char* scriptText;
  };
};

struct loadStruct {
  int error;
  int hasCallback;
  char* path;
};

struct eventsQueueItem {
  int eventType;
  eventsQueueItem* next;
  unsigned long serial;
  v8::Persistent<v8::Value> callback;
  union {
    emitStruct emit;
    evalStruct eval;
    loadStruct load;
  };
};

struct eventsQueue {
  eventsQueueItem* first;
  eventsQueueItem* last;
  pthread_mutex_t mutex;
};

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
  volatile int IDLE;
  volatile int ended;
  volatile int sigkill;
  int destroyed;
  int hasDestroyCallback;
  int hasIdleEventsListener;
  unsigned long threadMagicCookie;
  
  eventsQueue* threadEventsQueue;   //Jobs to run in the thread
  eventsQueue* processEventsQueue;  //Jobs to run in node's main thread
  
  pthread_cond_t idle_cv;
  pthread_mutex_t idle_mutex;
  
  v8::Isolate* isolate;
  v8::Persistent<v8::Value> nodeObject;
  v8::Persistent<v8::Value> destroyCallback;
  v8::Persistent<v8::Value> nodeDispatchEvents;
  
} typeThread;

//Type definitions END

//Prototypes BEGIN

static inline void beep (void);
static inline void qPush (eventsQueueItem*, eventsQueue*);
static inline eventsQueueItem* qPull (eventsQueue*);
static inline eventsQueueItem* nuQitem ();
static eventsQueue* nuQueue (void);
static eventsQueue* destroyQueue (eventsQueue*);
static inline typeThread* isAThread (v8::Handle<v8::Object>);
static inline void wakeUpThread (typeThread*, int);
static void* threadBootProc (void*);
static inline char* o2cstr (v8::Handle<v8::Value>);
static void eventLoop (typeThread*);
static void notifyIdle (typeThread*);
static void cleanUpAfterThreadUVCallback (uv_handle_t*);
static void cleanUpAfterThread (typeThread*);
static void Callback (
#ifdef TAGG_USE_LIBUV
  uv_async_t*
  #if defined(UV_VERSION_MAJOR) && (UV_VERSION_MAJOR == 0)
  , int
  #endif
#else
  EV_P_ ev_async*, int
#endif
);

#ifdef TAGG_USE_NEW_API
static void Puts (const v8::FunctionCallbackInfo<v8::Value>&);
static void NOP (const v8::FunctionCallbackInfo<v8::Value>&);
static void Destroy (const v8::FunctionCallbackInfo<v8::Value>&);
static void Eval (const v8::FunctionCallbackInfo<v8::Value>&);
static void Load (const v8::FunctionCallbackInfo<v8::Value>&);
static inline void pushEmitEvent (eventsQueue*, const v8::FunctionCallbackInfo<v8::Value>&);
static void processEmit (const v8::FunctionCallbackInfo<v8::Value>&);
static void threadEmit (const v8::FunctionCallbackInfo<v8::Value>&);
static void Create (const v8::FunctionCallbackInfo<v8::Value>&);
#else
static v8::Handle<v8::Value> Puts (const v8::Arguments &);
static v8::Handle<v8::Value> NOP (const v8::Arguments &);
static v8::Handle<v8::Value> Destroy (const v8::Arguments &);
static v8::Handle<v8::Value> Eval (const v8::Arguments &);
static v8::Handle<v8::Value> Load (const v8::Arguments &);
static inline void pushEmitEvent (eventsQueue*, const v8::Arguments &);
static v8::Handle<v8::Value> processEmit (const v8::Arguments &);
static v8::Handle<v8::Value> threadEmit (const v8::Arguments &);
static v8::Handle<v8::Value> Create (const v8::Arguments &);
#endif

void Init (v8::Handle<v8::Object>);

//Prototypes END


//Globals BEGIN

const char* k_TAGG_VERSION= "0.1.13";

static int TAGG_DEBUG= 0;
static bool useLocker;
static long int threadsCtr= 0;
static eventsQueue* qitemsStore= NULL;
static v8::Persistent<v8::Value> boot_js;
static v8::Persistent<v8::ObjectTemplate> threadTemplate;
static unsigned long serial= 0;

#include "boot.js.c"
#include "pool.js.c"

//Globals END

/*

cd deps/minifier/src
gcc minify.c -o minify
cat ../../../src/events.js | ./minify kEvents_js > ../../../src/kEvents_js
cat ../../../src/load.js | ./minify kLoad_js > ../../../src/kLoad_js
cat ../../../src/createPool.js | ./minify kCreatePool_js > ../../../src/kCreatePool_js
cat ../../../src/nextTick.js | ./minify kNextTick_js > ../../../src/kNextTick_js

//node-waf configure uninstall distclean configure build install

*/








//jejeje
static inline void beep (void) {
  printf("\x07"), fflush (stdout);  // que es lo mismo que \a
}








static inline void qPush (eventsQueueItem* qitem, eventsQueue* queue) {
  TAGG_DEBUG && printf("Q_PUSH\n");
  pthread_mutex_lock(&queue->mutex);
  qitem->next= NULL;
  if (queue->last) {
    assert(queue->last->next == NULL);
    queue->last->next= qitem;
  }
  else {
    assert(queue->first == NULL);
    queue->first= qitem;
  }
  queue->last= qitem;
  pthread_mutex_unlock(&queue->mutex);
}








static eventsQueueItem* qPull (eventsQueue* queue) {
  TAGG_DEBUG && printf("Q_PULL\n");
  pthread_mutex_lock(&queue->mutex);
  eventsQueueItem* qitem= NULL;
  if (queue->first != NULL) {
    qitem= queue->first;
    queue->first= qitem->next;
    if (queue->last == qitem) {
      queue->last= qitem->next;
    }
    qitem->next= NULL;
  }
  else {
    assert(queue->last == NULL);
  }
  pthread_mutex_unlock(&queue->mutex);
  return qitem;
}








static inline eventsQueueItem* nuQitem () {
  TAGG_DEBUG && printf("Q_NU_Q_ITEM\n");
  eventsQueueItem* qitem= qPull(qitemsStore);
  if (!qitem) {
    qitem= (eventsQueueItem*) calloc(1, sizeof(eventsQueueItem));
    beep();
  }
  qitem->serial= serial++;
  return qitem;
}








static eventsQueue* nuQueue (void) {
  TAGG_DEBUG && printf("Q_NU_QUEUE\n");
  eventsQueue* queue= (eventsQueue*) calloc(1, sizeof(eventsQueue));
  queue->first= queue->last= NULL;
  pthread_mutex_init(&(queue->mutex), NULL);
  return queue;
}








static eventsQueue* destroyQueue (eventsQueue* queue) {
  eventsQueueItem* qitem;
  while ((qitem= qPull(queue))) qPush(qitem, qitemsStore);
  pthread_mutex_destroy(&(queue->mutex));
  free(queue);
  return NULL;
}







//Llamar a un método de la thread con el 'this' (receiver) mal puesto es bombazo seguro, por eso esto.
static typeThread* isAThread (v8::Handle<v8::Object> receiver) {
  typeThread* thread= NULL;
  if (receiver->IsObject()) {
#ifdef TAGG_USE_NEW_API
    v8::Local<v8::Value> ptr= receiver->GetHiddenValue(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "ptr"));
    thread= (typeThread*) ((uintptr_t) ptr->ToNumber()->Value());
#else
    if (receiver->InternalFieldCount() == 1)
      thread= (typeThread*) receiver->GetPointerFromInternalField(0);
#endif
    assert(thread != NULL);
    assert(thread->threadMagicCookie == kThreadMagicCookie);
    return thread;
  }
  return NULL;
}







//Se encarga de poner en marcha la thread si es que estaba durmiendo
static void wakeUpThread (typeThread* thread, int sigkill) {

//Esto se ejecuta siempre en node's main thread

  TAGG_DEBUG && printf("THREAD %ld wakeUpThread(sigkill=%d) #1\n", thread->id, sigkill);
  
  //Cogiendo este lock sabemos que la thread o no ha salido aún
  //del event loop o está parada en wait/sleep/idle
  pthread_mutex_lock(&thread->idle_mutex);
  
  TAGG_DEBUG && printf("THREAD %ld wakeUpThread(sigkill=%d) #2\n", thread->id, sigkill);
  //Estamos seguros de que no se está tocando thread->IDLE
  //xq tenemos el lock nosotros y sólo se toca con el lock puesto
  
  //Es un error volver llamar a esto después de un sigkill
  assert(!thread->sigkill);
  thread->sigkill= sigkill;
  if (thread->IDLE) {
    //estaba parada, hay que ponerla en marcha
    TAGG_DEBUG && printf("THREAD %ld wakeUpThread(sigkill=%d) #3\n", thread->id, sigkill);
    pthread_cond_signal(&thread->idle_cv);
  }
  //Hay que volver a soltar el lock
  pthread_mutex_unlock(&thread->idle_mutex);
  TAGG_DEBUG && printf("THREAD %ld wakeUpThread(sigkill=%d) #5 EXIT\n", thread->id, sigkill);
/*
  if (thread->sigkill == kKillRudely) {
    thread->isolate->TerminateExecution();
    printf("THREAD %ld wakeUpThread(sigkill=%d) TerminateExecution() #6 EXIT\n", thread->id, sigkill);
  }
*/
}





#if defined(TAGG_USE_ALLOCATOR)
#if !defined(TAGG_USE_DEFAULT_ALLOCATOR)

//See https://github.com/v8/v8/blob/1440cd3d833c7bca7777232b5fd754352010aa3e/samples/shell.cc#L66

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};

#endif
#endif







//Esto es lo primero que se ejecuta en la(s) thread(s) al nacer.
//Básicamente inicializa lo necesario y se entra en el eventloop
static void* threadBootProc (void* arg) {

  int dummy;
  typeThread* thread= (typeThread*) arg;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &dummy);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &dummy);
  
  TAGG_DEBUG && printf("THREAD %ld BOOTPROC ENTER\n", thread->id);
  
  assert(v8::Isolate::GetCurrent() == NULL);
  
#ifdef TAGG_USE_ALLOCATOR

  v8::Isolate::CreateParams create_params;
  
  #ifdef TAGG_USE_DEFAULT_ALLOCATOR
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  #else
    ArrayBufferAllocator wtf;
    create_params.array_buffer_allocator= &wtf;
  #endif
  
  thread->isolate= v8::Isolate::New(create_params);
  
#else
  thread->isolate= v8::Isolate::New();
#endif

#ifdef TAGG_USE_NEW_API
  thread->isolate->SetData(1, thread);
#else
  thread->isolate->SetData(thread);
#endif

  if (useLocker) {
    v8::Locker wtf(thread->isolate);
    eventLoop(thread);
  }
  else
    eventLoop(thread);
  
  assert(v8::Isolate::GetCurrent() == NULL);
  
  thread->isolate->Dispose();
  thread->ended= 1;
  WAKEUP_NODE_EVENT_LOOP
  
  TAGG_DEBUG && printf("THREAD %ld BOOTPROC EXIT\n", thread->id);
  return NULL;
}








static inline char* o2cstr (v8::Handle<v8::Value> o) {
  v8::String::Utf8Value utf8(o);
  long len= utf8.length();
  char* r= (char*) malloc( (len + 1) * sizeof(char));
  memcpy(r, *utf8, len);
  r[len]= 0;
  return r;
}








// The thread's eventloop runs in the thread(s) not in node's main thread
static void eventLoop (typeThread* thread) {
  TAGG_DEBUG && printf("THREAD %ld EVENTLOOP ENTER\n", thread->id);
#ifdef TAGG_USE_NEW_API
  v8::Isolate* iso= thread->isolate;
  v8::Isolate::Scope isolate_scope(iso);
  v8::HandleScope scope1(iso);
  v8::Local<v8::Context> context= v8::Context::New(iso);
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Object> global= context->Global();
  global->Set(v8::String::NewFromUtf8(iso, "puts"), v8::FunctionTemplate::New(iso, Puts)->GetFunction());
  v8::Local<v8::Object> threadObject= v8::Object::New(iso);
  threadObject->Set(v8::String::NewFromUtf8(iso, "id"), v8::Number::New(iso, thread->id));
  threadObject->Set(v8::String::NewFromUtf8(iso, "version"),v8::String::NewFromUtf8(iso, k_TAGG_VERSION));
  threadObject->Set(v8::String::NewFromUtf8(iso, "emit"), v8::FunctionTemplate::New(iso, threadEmit)->GetFunction());
  v8::Local<v8::Object> script= v8::Local<v8::Object>::New(iso, v8::Script::Compile(v8::String::NewFromUtf8(iso, kBoot_js))->Run()->ToObject());
  v8::Local<v8::Object> r= script->CallAsFunction(threadObject, 0, NULL)->ToObject();
  v8::Local<v8::Object> dnt= r->Get(v8::String::NewFromUtf8(iso, "dnt"))->ToObject();
  v8::Local<v8::Object> dev= r->Get(v8::String::NewFromUtf8(iso, "dev"))->ToObject();
#else
  thread->isolate->Enter();
  v8::Persistent<v8::Context> context= v8::Context::New();
  context->Enter();
  {
  v8::HandleScope scope1;
  v8::Local<v8::Object> global= context->Global();
  global->Set(v8::String::NewSymbol("puts"), v8::FunctionTemplate::New(Puts)->GetFunction());
  v8::Local<v8::Object> threadObject= v8::Object::New();
  threadObject->Set(v8::String::NewSymbol("id"), v8::Number::New(thread->id));
  threadObject->Set(v8::String::NewSymbol("version"),v8::String::New(k_TAGG_VERSION));
  threadObject->Set(v8::String::NewSymbol("emit"), v8::FunctionTemplate::New(threadEmit)->GetFunction());
  v8::Local<v8::Object> script= v8::Local<v8::Object>::New(v8::Script::Compile(v8::String::New(kBoot_js))->Run()->ToObject());
  v8::Local<v8::Object> r= script->CallAsFunction(threadObject, 0, NULL)->ToObject();
  v8::Local<v8::Object> dnt= r->Get(v8::String::NewSymbol("dnt"))->ToObject();
  v8::Local<v8::Object> dev= r->Get(v8::String::NewSymbol("dev"))->ToObject();
#endif

  //SetFatalErrorHandler(FatalErrorCB);
  
  double ntql;
  eventsQueueItem* event= NULL;
  while (1) {
    v8::TryCatch onError;
    TAGG_DEBUG && printf("THREAD %ld IN WHILE(1)\n", thread->id);
    
    if (thread->sigkill == kKillRudely) break;
    
    if (!event) event= qPull(thread->threadEventsQueue);
    if (event) {
    
      TAGG_DEBUG && printf("THREAD %ld QITEM #%ld\n", thread->id, event->serial);
      
      if (event->eventType == kEventTypeLoad) {
#ifdef TAGG_USE_NEW_API
        v8::HandleScope scope(iso);
#else
        v8::HandleScope scope;
#endif
        
        v8::Local<v8::Script> script;
        v8::Local<v8::Value> resultado;
        
        TAGG_DEBUG && printf("THREAD %ld QITEM LOAD\n", thread->id);
        
        char* buf= NULL;
        assert(event->load.path != NULL);
        FILE* fp= fopen(event->load.path, "rb");
        free(event->load.path);
        
        if (fp) {
          fseek(fp, 0, SEEK_END);
          long len= ftell(fp);
          rewind(fp); //fseek(fp, 0, SEEK_SET);
          buf= (char*) calloc(len + 1, sizeof(char)); // +1 to get null terminated string
          fread(buf, len, 1, fp);
          fclose(fp);
        }
        
        if (buf != NULL) {
#ifdef TAGG_USE_NEW_API
          script= v8::Script::Compile(v8::String::NewFromUtf8(iso, buf));
#else
          script= v8::Script::Compile(v8::String::New(buf));
#endif
          free(buf);
          if (!onError.HasCaught()) resultado= script->Run();
          event->load.error= onError.HasCaught() ? 1 : 0;
        }
        else {
          event->load.error= 2;
        }
        
        if (event->load.hasCallback) {
          if (!event->load.error)
            event->eval.resultado= o2cstr(resultado);
          else if (event->load.error == 1)
            event->eval.resultado= o2cstr(onError.Exception());
          else
            event->eval.resultado= strdup("fopen(path) error");
          event->eventType= kEventTypeEval;
          qPush(event, thread->processEventsQueue);
          WAKEUP_NODE_EVENT_LOOP
        }
        else {
          qPush(event, qitemsStore);
        }
        
        if (onError.HasCaught()) onError.Reset();
      }
      else if (event->eventType == kEventTypeEval) {
#ifdef TAGG_USE_NEW_API
        v8::HandleScope scope(iso);
#else
        v8::HandleScope scope;
#endif
        
        v8::Local<v8::Script> script;
        v8::Local<v8::Value> resultado;
        
        TAGG_DEBUG && printf("THREAD %ld QITEM EVAL\n", thread->id);
#ifdef TAGG_USE_NEW_API
        script= v8::Script::Compile(v8::String::NewFromUtf8(iso, event->eval.scriptText));
#else
        script= v8::Script::Compile(v8::String::New(event->eval.scriptText));
#endif
        free(event->eval.scriptText);
      
        if (!onError.HasCaught()) resultado= script->Run();
        if (event->eval.hasCallback) {
          event->eval.error= onError.HasCaught() ? 1 : 0;
          if (!event->eval.error)
            event->eval.resultado= o2cstr(resultado);
          else
            event->eval.resultado= o2cstr(onError.Exception());
          event->eventType= kEventTypeEval;
          qPush(event, thread->processEventsQueue);
          WAKEUP_NODE_EVENT_LOOP
        }
        else {
          qPush(event, qitemsStore);
        }
        
        if (onError.HasCaught()) onError.Reset();
      }
      else if (event->eventType == kEventTypeEmit) {
#ifdef TAGG_USE_NEW_API
        v8::HandleScope scope(iso);
#else
        v8::HandleScope scope;
#endif
        v8::Local<v8::Array> array;
        v8::Local<v8::Value> args[2];
        
        TAGG_DEBUG && printf("THREAD %ld QITEM EVENT #%ld\n", thread->id, event->serial);
        
        assert(event->emit.eventName != NULL);
        
#ifdef TAGG_USE_NEW_API
        args[0]= v8::String::NewFromUtf8(iso, event->emit.eventName);
        free(event->emit.eventName);
        args[1]= array= v8::Array::New(iso, event->emit.argc);
        if (event->emit.argc) {
          int i= 0;
          while (i < event->emit.argc) {
            array->Set(i, v8::String::NewFromUtf8(iso, event->emit.argv[i]));
            free(event->emit.argv[i]);
            i++;
          }
          free(event->emit.argv);
        }
#else
        args[0]= v8::String::New(event->emit.eventName);
        free(event->emit.eventName);
        args[1]= array= v8::Array::New(event->emit.argc);
        if (event->emit.argc) {
          int i= 0;
          while (i < event->emit.argc) {
            array->Set(i, v8::String::New(event->emit.argv[i]));
            free(event->emit.argv[i]);
            i++;
          }
          free(event->emit.argv);
        }
#endif
        dev->CallAsFunction(global, 2, args);
        qPush(event, qitemsStore);
      }
      else {
        assert(0);
      }
    }
    else
      TAGG_DEBUG && printf("THREAD %ld NO QITEM\n", thread->id);
    
    if (thread->sigkill == kKillRudely) break;
    
    TAGG_DEBUG && printf("THREAD %ld NTQL\n", thread->id);
    ntql= dnt->CallAsFunction(global, 0, NULL)->ToNumber()->Value();
    if (onError.HasCaught()) onError.Reset();
    
    event= NULL;
    if (ntql) continue;
    event= qPull(thread->threadEventsQueue);
    if (event) continue;
    if (thread->sigkill) break;
    
    TAGG_DEBUG && printf("THREAD %ld : NO NTQL AND NO QITEM\n", thread->id);

    //v8::V8::IdleNotification();
    
    TAGG_DEBUG && printf("THREAD %ld BEFORE MUTEX\n", thread->id);
    //cogemos el lock para
    //por un lado poder mirar si hay cosas en la queue sabiendo
    //que nadie la está tocando
    //y por otro lado para poder tocar thread->IDLE sabiendo
    //que nadie la está mirando mientras la tocamos.
    pthread_mutex_lock(&thread->idle_mutex);
    TAGG_DEBUG && printf("THREAD %ld TIENE threadEventsQueue_MUTEX\n", thread->id);
    event= qPull(thread->threadEventsQueue);
    //aquí tenemos acceso exclusivo a threadEventsQueue y a thread->IDLE
    if (!event && !thread->sigkill) {
      //sólo se entra aquí si no hay nada en la queue y no hay sigkill
      //hemos avisado con thread->IDLE de que nos quedamos parados
      // para que sepan que nos han de despertar
      thread->IDLE= 1;
      if (thread->hasIdleEventsListener) notifyIdle(thread);
      TAGG_DEBUG && printf("THREAD %ld SLEEP\n", thread->id);
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
    TAGG_DEBUG && printf("THREAD %ld WAKE UP\n", thread->id);
    //lo soltamos
    pthread_mutex_unlock(&thread->idle_mutex);
    TAGG_DEBUG && printf("THREAD %ld SUELTA threadEventsQueue_mutex\n", thread->id);
    
  }
  
#ifdef TAGG_USE_NEW_API
#else
  context->Exit();
  context.Dispose();
  }
  thread->isolate->Exit();
#endif
  TAGG_DEBUG && printf("THREAD %ld EVENTLOOP EXIT\n", thread->id);
}







//Cuando una thread se echa a dormir esto lo debe notificar a node. OJO TODO
static void notifyIdle (typeThread* thread) {
  printf("*** notifyIdle()\n");
}








//Esto es por culpa de libuv que se empeña en tener un callback de terminación. Al parecer...
static void cleanUpAfterThreadUVCallback (uv_handle_t* arg) {

  typeThread* thread= (typeThread*) arg;
  
#ifdef TAGG_USE_NEW_API
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  
  TAGG_DEBUG && printf("THREAD %ld cleanUpAfterThreadUVCallback()\n", thread->id);
  
  if (thread->hasDestroyCallback) {
    v8::Local<v8::Value> cb;
    cb= v8::Local<v8::Value>::New(v8::Isolate::GetCurrent(), thread->destroyCallback);
    assert(cb->IsFunction());
    cb->ToObject()->CallAsFunction(v8::Isolate::GetCurrent()->GetCurrentContext()->Global(), 0, NULL);
    thread->destroyCallback.Reset();
  }
  
  thread->nodeDispatchEvents.Reset();
  thread->nodeObject.Reset();
#else
  v8::HandleScope scope;
  
  TAGG_DEBUG && printf("THREAD %ld cleanUpAfterThreadCallback()\n", thread->id);
  
  if (thread->hasDestroyCallback) {
    thread->destroyCallback->ToObject()->CallAsFunction(v8::Context::GetCurrent()->Global(), 0, NULL);
    thread->destroyCallback.Dispose();
  }
  
  thread->nodeDispatchEvents.Dispose();
  thread->nodeObject.Dispose();
#endif

  free(thread);
}








//Deshacerse de todo, lo que se pueda guardar se guarda para reutilizarlo
static void cleanUpAfterThread (typeThread* thread) {
  
  TAGG_DEBUG && printf("THREAD %ld cleanUpAfterThread() IN MAIN THREAD #1\n", thread->id);
  TAGG_DEBUG && printf("THREAD %ld cleanUpAfterThread() destroyQueue(thread->threadEventsQueue)\n", thread->id);
  thread->threadEventsQueue= destroyQueue(thread->threadEventsQueue);
  TAGG_DEBUG && printf("THREAD %ld cleanUpAfterThread() destroyQueue(thread->processEventsQueue)\n", thread->id);
  thread->processEventsQueue= destroyQueue(thread->processEventsQueue);
  pthread_cond_destroy(&(thread->idle_cv));
  pthread_mutex_destroy(&(thread->idle_mutex));
  
  if (thread->ended) {
    // Esta thread llegó a funcionar alguna vez
    // hay que apagar uv antes de poder hacer free(thread)
    // free(thread) se hará en cleanUpAfterThreadUVCallback xq uv_close la va a llamar
    
    TAGG_DEBUG && printf("THREAD %ld cleanUpAfterThread() FREE IN UV CALLBACK #2\n", thread->id);
    
#ifdef TAGG_USE_LIBUV
    uv_close((uv_handle_t*) &thread->async_watcher, cleanUpAfterThreadUVCallback);
    //uv_unref(&thread->async_watcher);
#else
    ev_async_stop(EV_DEFAULT_UC_ &thread->async_watcher);
    ev_unref(EV_DEFAULT_UC);
    cleanUpAfterThreadUVCallback((uv_handle_t*) thread);
#endif

  }
  else {
    //Esta thread nunca ha llegado a arrancar
    //Seguramente venimos de un error en thread.create())
    TAGG_DEBUG && printf("THREAD %ld cleanUpAfterThread() FREE HERE #3\n", thread->id);
    free(thread);
  }
}








// C callback that runs in node's main thread. This is called by node's event loop
// when the thread tells it to do so. This is the one responsible for
// calling the thread's JS callback in node's js context in node's main thread.
static void Callback (
#ifdef TAGG_USE_LIBUV
  uv_async_t* watcher
  #if defined(UV_VERSION_MAJOR) && (UV_VERSION_MAJOR == 0)
  , int status
  #endif
#else
  EV_P_ ev_async* watcher, int status
#endif
) {
  
  eventsQueueItem* event;
  typeThread* thread= (typeThread*) watcher;
  
#ifdef TAGG_USE_NEW_API
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  
  v8::Isolate* iso= v8::Isolate::GetCurrent();
  
  v8::Local<v8::Value> cb;
  v8::Local<v8::Value> that;
  v8::Local<v8::Array> array;
  v8::Local<v8::Value> args[2];
  v8::Local<v8::Value> null= v8::Local<v8::Value>::New(iso, v8::Null(iso));
  
  assert(thread != NULL);
  assert(!thread->destroyed);
  
  TAGG_DEBUG && printf("UV CALLBACK FOR THREAD %ld BEGIN\n", thread->id);
  
  v8::TryCatch onError;
  while ((event= qPull(thread->processEventsQueue))) {
  
    TAGG_DEBUG && printf("UV CALLBACK FOR THREAD %ld GOT EVENT #%ld\n", thread->id, event->serial);

    if (event->eventType == kEventTypeEval) {
    
      TAGG_DEBUG && printf("CALLBACK kEventTypeEval IN MAIN THREAD\n");
      
      assert(event->eval.hasCallback);
      assert(event->eval.resultado != NULL);
      
      if (event->eval.error) {
        args[0]= v8::Exception::Error(v8::String::NewFromUtf8(iso, event->eval.resultado));
        args[1]= null;
      }
      else {
        args[0]= null;
        args[1]= v8::String::NewFromUtf8(iso, event->eval.resultado);
      }
      free(event->eval.resultado);
      
      cb= v8::Local<v8::Value>::New(iso, event->callback);
      that= v8::Local<v8::Value>::New(iso, thread->nodeObject);
      assert(that->IsObject());
      assert(cb->IsFunction());
      cb->ToObject()->CallAsFunction(that->ToObject(), 2, args);
      event->callback.Reset();
      event->eventType = kEventTypeEmpty;
      qPush(event, qitemsStore);
      
      if (onError.HasCaught()) {
        node::FatalException(onError);
        return;
      }
    }
    else if (event->eventType == kEventTypeEmit) {
    
      TAGG_DEBUG && printf("CALLBACK kEventTypeEmit IN MAIN THREAD\n");
      
      args[0]= v8::String::NewFromUtf8(iso, event->emit.eventName);
      free(event->emit.eventName);
      array= v8::Array::New(iso, event->emit.argc);
      args[1]= array;
      if (event->emit.argc) {
        int i= 0;
        while (i < event->emit.argc) {
          array->Set(i, v8::String::NewFromUtf8(iso, event->emit.argv[i]));
          free(event->emit.argv[i]);
          i++;
        }
        free(event->emit.argv);
      }
      cb= v8::Local<v8::Value>::New(iso, thread->nodeDispatchEvents);
      cb->ToObject()->CallAsFunction(iso->GetCurrentContext()->Global(), 2, args);
      
      event->eventType = kEventTypeEmpty;
      qPush(event, qitemsStore);
    }
    else {
      assert(0);
    }
    
  }
#else
  v8::HandleScope scope;
  
  v8::Local<v8::Array> array;
  v8::Local<v8::Value> args[2];
  v8::Local<v8::Value> null= v8::Local<v8::Value>::New(v8::Null());
  
  assert(thread != NULL);
  assert(!thread->destroyed);
  
  TAGG_DEBUG && printf("UV CALLBACK FOR THREAD %ld BEGIN\n", thread->id);
  
  v8::TryCatch onError;
  while ((event= qPull(thread->processEventsQueue))) {
  
    TAGG_DEBUG && printf("UV CALLBACK FOR THREAD %ld GOT EVENT #%ld\n", thread->id, event->serial);

    if (event->eventType == kEventTypeEval) {
    
      TAGG_DEBUG && printf("CALLBACK eventTypeEval IN MAIN THREAD\n");
      
      assert(event->eval.hasCallback);
      assert(event->eval.resultado != NULL);
      
      if (event->eval.error) {
        args[0]= v8::Exception::Error(v8::String::New(event->eval.resultado));
        args[1]= null;
      }
      else {
        args[0]= null;
        args[1]= v8::String::New(event->eval.resultado);
      }
      free(event->eval.resultado);
      
      assert(event->callback->IsFunction());
      event->callback->ToObject()->CallAsFunction(thread->nodeObject->ToObject(), 2, args);
      event->callback.Dispose();
      event->eventType = kEventTypeEmpty;
      qPush(event, qitemsStore);
      
      if (onError.HasCaught()) {
        node::FatalException(onError);
        return;
      }
    }
    else if (event->eventType == kEventTypeEmit) {
    
      TAGG_DEBUG && printf("CALLBACK kEventTypeEmit IN MAIN THREAD\n");
      
      args[0]= v8::String::New(event->emit.eventName);
      free(event->emit.eventName);
      array= v8::Array::New(event->emit.argc);
      args[1]= array;
      if (event->emit.argc) {
        int i= 0;
        while (i < event->emit.argc) {
          array->Set(i, v8::String::New(event->emit.argv[i]));
          free(event->emit.argv[i]);
          i++;
        }
        free(event->emit.argv);
      }
      thread->nodeDispatchEvents->ToObject()->CallAsFunction(v8::Context::GetCurrent()->Global(), 2, args);
      
      event->eventType = kEventTypeEmpty;
      qPush(event, qitemsStore);
    }
    else {
      assert(0);
    }
    
  }
#endif

  if (thread->sigkill && thread->ended) {
    TAGG_DEBUG && printf("UV CALLBACK FOR THREAD %ld CALLED cleanUpAfterThread()\n", thread->id);
    //pthread_cancel(thread->thread);
    thread->destroyed= 1;
    cleanUpAfterThread(thread);
  }
  
  TAGG_DEBUG && printf("UV CALLBACK FOR THREAD %ld END\n", thread->id);
}







//printf de andar por casa
#ifdef TAGG_USE_NEW_API
static void Puts (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
#else
static v8::Handle<v8::Value> Puts (const v8::Arguments &args) {
  v8::HandleScope scope;
#endif
  int i= 0;
  while (i < args.Length()) {
    v8::String::Utf8Value c_str(args[i]);
    fputs(*c_str, stdout);
    i++;
  }
  fflush(stdout);
#ifdef TAGG_USE_NEW_API
  //args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
#else
  return v8::Undefined();
#endif
}








// Calling a method of a destroyed thread throws an error.
#ifdef TAGG_USE_NEW_API
static void NOP (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* iso= args.GetIsolate();
  v8::HandleScope scope(iso);
  iso->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(iso, "This thread has been destroyed.")));
}
#else
static v8::Handle<v8::Value> NOP (const v8::Arguments &args) {
  v8::HandleScope scope;
  return v8::ThrowException(v8::Exception::TypeError(v8::String::New("This thread has been destroyed")));
}
#endif







// Tell a thread to quit, either nicely or rudely.
#ifdef TAGG_USE_NEW_API
static void Destroy (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* iso= args.GetIsolate();
  v8::HandleScope scope(iso);
#else
static v8::Handle<v8::Value> Destroy (const v8::Arguments &args) {
  v8::HandleScope scope;
#endif

  //thread.destroy() or thread.destroy(0) means nicely (the default)
  //thread destroy(1) means rudely.
  //When done nicely the thread will quit only if/when there aren't anymore jobs pending
  //in its jobsQueue nor nextTick()ed functions to execute in the nextTick queue _ntq[]
  //When done rudely it will try to exit the event loop regardless.
  //ToDo: If the thread is stuck in a ` while (1) ; ` or something this won't work...
  
  //TODO: Hay que comprobar que this en un objeto y que tiene hiddenRefTotypeThread_symbol y que no es nil
  //TODO: Aquí habría que usar static void TerminateExecution(int thread_id);
  //TODO: static void v8::V8::TerminateExecution  ( Isolate *   isolate= NULL   )
  
  typeThread* thread= isAThread(args.This());
  if (!thread) {
#ifdef TAGG_USE_NEW_API
    args.GetReturnValue().Set(iso->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(iso, "thread.destroy(): the receiver must be a thread object"))));
    return;
#else
    return v8::ThrowException(v8::Exception::TypeError(v8::String::New("thread.destroy(): the receiver must be a thread object")));
#endif
  }
  
  int nuSigkill= kKillNicely;
  if (args.Length()) {
    nuSigkill= args[0]->ToNumber()->Value() ? kKillRudely : kKillNicely;
  }
  
  thread->hasDestroyCallback= (args.Length() > 1) && (args[1]->IsFunction());
  if (thread->hasDestroyCallback) {
#ifdef TAGG_USE_NEW_API
    thread->destroyCallback.Reset(iso, args[1]);
#else
    thread->destroyCallback= v8::Persistent<v8::Object>::New(args[1]->ToObject());
#endif
  }
  
  if (TAGG_DEBUG) {
    const char* str= (nuSigkill == kKillNicely ? "NICELY" : "RUDELY");
    printf("THREAD %ld DESTROY(%s) #1\n", thread->id, str);
  }
  
  wakeUpThread(thread, nuSigkill);
  
#ifdef TAGG_USE_NEW_API
  v8::Local<v8::Object> nodeObject;
  nodeObject= v8::Local<v8::Value>::New(iso, thread->nodeObject)->ToObject();
  nodeObject->Set(v8::String::NewFromUtf8(iso, "load"), v8::FunctionTemplate::New(iso, NOP)->GetFunction());
  nodeObject->Set(v8::String::NewFromUtf8(iso, "eval"), v8::FunctionTemplate::New(iso, NOP)->GetFunction());
  nodeObject->Set(v8::String::NewFromUtf8(iso, "emit"), v8::FunctionTemplate::New(iso, NOP)->GetFunction());
  nodeObject->Set(v8::String::NewFromUtf8(iso, "destroy"), v8::FunctionTemplate::New(iso, NOP)->GetFunction());
#else
  thread->nodeObject->ToObject()->Set(v8::String::NewSymbol("load"), v8::FunctionTemplate::New(NOP)->GetFunction());
  thread->nodeObject->ToObject()->Set(v8::String::NewSymbol("eval"), v8::FunctionTemplate::New(NOP)->GetFunction());
  thread->nodeObject->ToObject()->Set(v8::String::NewSymbol("emit"), v8::FunctionTemplate::New(NOP)->GetFunction());
  thread->nodeObject->ToObject()->Set(v8::String::NewSymbol("destroy"), v8::FunctionTemplate::New(NOP)->GetFunction());
  return v8::Undefined();
#endif
}








// Eval: Pushes an eval job to the threadEventsQueue.
#ifdef TAGG_USE_NEW_API
static void Eval (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
#else
static v8::Handle<v8::Value> Eval (const v8::Arguments &args) {
  v8::HandleScope scope;
#endif

  if (!args.Length()) {
#ifdef TAGG_USE_NEW_API
    args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(args.GetIsolate(), "thread.eval(program [,callback]): missing arguments"))));
    return;
#else
    return v8::ThrowException(v8::Exception::TypeError(v8::String::New("thread.eval(program [,callback]): missing arguments")));
#endif
  }
  
  typeThread* thread= isAThread(args.This());
  if (!thread) {
#ifdef TAGG_USE_NEW_API
    args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(args.GetIsolate(), "thread.eval(): the receiver must be a thread object"))));
    return;
#else
    return v8::ThrowException(v8::Exception::TypeError(v8::String::New("thread.eval(): the receiver must be a thread object")));
#endif
  }

  eventsQueueItem* event= nuQitem();
  event->eval.hasCallback= (args.Length() > 1) && (args[1]->IsFunction());
  if (event->eval.hasCallback) {
#ifdef TAGG_USE_NEW_API
    event->callback.Reset(args.GetIsolate(), args[1]);
#else
    event->callback= v8::Persistent<v8::Object>::New(args[1]->ToObject());
#endif
  }
  event->eval.scriptText= o2cstr(args[0]);
  event->eventType= kEventTypeEval;
  qPush(event, thread->threadEventsQueue);
  wakeUpThread(thread, thread->sigkill);
#ifdef TAGG_USE_NEW_API
  args.GetReturnValue().Set(args.This());
#else
  return args.This();
#endif
}








// Load: emits a kEventTypeLoad event to the thread
#ifdef TAGG_USE_NEW_API
static void Load (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
#else
static v8::Handle<v8::Value> Load (const v8::Arguments &args) {
  v8::HandleScope scope;
#endif

  if (!args.Length()) {
  
#ifdef TAGG_USE_NEW_API
    args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(args.GetIsolate(), "thread.load(filename [,callback]): missing arguments"))));
    return;
#else
    return v8::ThrowException(v8::Exception::TypeError(v8::String::New("thread.load(filename [,callback]): missing arguments")));
#endif

  }

  typeThread* thread= isAThread(args.This());
  if (!thread) {
  
#ifdef TAGG_USE_NEW_API
    args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(args.GetIsolate(), "thread.load(): the receiver must be a thread object"))));
    return;
#else
    return v8::ThrowException(v8::Exception::TypeError(v8::String::New("thread.load(): the receiver must be a thread object")));
#endif

  }
  
  eventsQueueItem* event= nuQitem();
  event->load.path= o2cstr(args[0]);
  event->load.hasCallback= ((args.Length() > 1) && (args[1]->IsFunction()));
  if (event->load.hasCallback) {
  
#ifdef TAGG_USE_NEW_API
    event->callback.Reset(args.GetIsolate(), args[1]);
#else
    event->callback= v8::Persistent<v8::Object>::New(args[1]->ToObject());
#endif

  }
  
  event->eventType= kEventTypeLoad;
  qPush(event, thread->threadEventsQueue);
  wakeUpThread(thread, thread->sigkill);
  
#ifdef TAGG_USE_NEW_API
  args.GetReturnValue().Set(args.This());
#else
  return args.This();
#endif
}








//por que casi todo el código es idéntico en processEmit y threadEmit
#ifdef TAGG_USE_NEW_API
static inline void pushEmitEvent (eventsQueue* queue, const v8::FunctionCallbackInfo<v8::Value>& args) {
#else
static inline void pushEmitEvent (eventsQueue* queue, const v8::Arguments &args) {
#endif

  eventsQueueItem* event= nuQitem();
  event->emit.eventName= o2cstr(args[0]);
  event->emit.argc= (args.Length() > 1) ? (args.Length() - 1) : 0;
  if (event->emit.argc) {
    event->emit.argv= (char**) malloc(event->emit.argc * sizeof(char*));
    int i= 0;
    while (i < event->emit.argc) {
      event->emit.argv[i]= o2cstr(args[i+1]);
      i++;
    }
  }
  
  TAGG_DEBUG && printf("PROCESS EMIT TO THREAD #%ld\n", event->serial);
  
  event->eventType= kEventTypeEmit;
  qPush(event, queue);
  
}







//La que emite los events de node hacia las threads
#ifdef TAGG_USE_NEW_API
static void processEmit (const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!args.Length()) return args.GetReturnValue().Set(args.This());
  typeThread* thread= isAThread(args.This());
  if (!thread) {
    args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(args.GetIsolate(), "thread.emit(): 'this' must be a thread object"))));
    return;
  }
#else
static v8::Handle<v8::Value> processEmit (const v8::Arguments &args) {
  if (!args.Length()) return args.This();
  typeThread* thread= isAThread(args.This());
  if (!thread) {
    return v8::ThrowException(v8::Exception::TypeError(v8::String::New("thread.emit(): 'this' must be a thread object")));
  }
#endif

  pushEmitEvent(thread->threadEventsQueue, args);
  wakeUpThread(thread, thread->sigkill);
  
#ifdef TAGG_USE_NEW_API
  args.GetReturnValue().Set(args.This());
#else
  return args.This();
#endif
}







//La que emite los events de las threads hacia node, se ejecuta en las threads.
#ifdef TAGG_USE_NEW_API
static void threadEmit (const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!args.Length()) return args.GetReturnValue().Set(args.This());
  typeThread* thread= (typeThread*) args.GetIsolate()->GetData(1);
#else
static v8::Handle<v8::Value> threadEmit (const v8::Arguments &args) {
  if (!args.Length()) return args.This();
  typeThread* thread= (typeThread*) v8::Isolate::GetCurrent()->GetData();
#endif

  assert(thread != NULL);
  assert(thread->threadMagicCookie == kThreadMagicCookie);
  pushEmitEvent(thread->processEventsQueue, args);
  WAKEUP_NODE_EVENT_LOOP
#ifdef TAGG_USE_NEW_API
  args.GetReturnValue().Set(args.This());
#else
  return args.This();
#endif
}








//Se ejecuta al hacer tagg.create(): Creates and launches a new isolate in a new background thread.
#ifdef TAGG_USE_NEW_API
static void Create (const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::HandleScope scope(args.GetIsolate());
    v8::Isolate* iso= args.GetIsolate();
#else
static v8::Handle<v8::Value> Create (const v8::Arguments &args) {
    v8::HandleScope scope;
#endif
    
    typeThread* thread= (typeThread*) calloc(1, sizeof (typeThread));
    thread->id= threadsCtr++;
    thread->threadMagicCookie= kThreadMagicCookie;
    thread->threadEventsQueue= nuQueue();
    thread->processEventsQueue= nuQueue();

    pthread_cond_init(&(thread->idle_cv), NULL);
    pthread_mutex_init(&(thread->idle_mutex), NULL);
    
    char* errstr;
    //pthread_detach(pthread_t thread); ???
    int err= pthread_create(&(thread->thread), NULL, threadBootProc, thread);
    if (err) {
      errstr= strerror(err);
      printf("THREAD %ld PTHREAD_CREATE() ERROR %d : %s\n", thread->id, err, errstr);
      //Algo ha ido mal, toca deshacer todo
      TAGG_DEBUG && printf("CALLED cleanUpAfterThread %ld FROM CREATE()\n", thread->id);
      cleanUpAfterThread(thread);
#ifdef TAGG_USE_NEW_API
      args.GetReturnValue().Set(iso->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(iso, "create(): error in pthread_create()"))));
      return;
#else
      return v8::ThrowException(v8::Exception::TypeError(v8::String::New("create(): error in pthread_create()")));
#endif
    }

#ifdef TAGG_USE_NEW_API
    v8::Local<v8::Object> nodeObject= v8::Object::New(iso);
    nodeObject->SetHiddenValue(v8::String::NewFromUtf8(iso, "ptr"), v8::Number::New(iso, (double) ((uintptr_t) thread)));
    nodeObject->Set(v8::String::NewFromUtf8(iso, "load"), v8::FunctionTemplate::New(iso, Load)->GetFunction());
    nodeObject->Set(v8::String::NewFromUtf8(iso, "eval"), v8::FunctionTemplate::New(iso, Eval)->GetFunction());
    nodeObject->Set(v8::String::NewFromUtf8(iso, "emit"), v8::FunctionTemplate::New(iso, processEmit)->GetFunction());
    nodeObject->Set(v8::String::NewFromUtf8(iso, "destroy"), v8::FunctionTemplate::New(iso, Destroy)->GetFunction());
    nodeObject->Set(v8::String::NewFromUtf8(iso, "id"), v8::Integer::New(iso, thread->id));
    nodeObject->Set(v8::String::NewFromUtf8(iso, "version"), v8::String::NewFromUtf8(iso, k_TAGG_VERSION));
    v8::Local<v8::Value> boot= v8::Local<v8::Value>::New(iso, boot_js);
    thread->nodeDispatchEvents.Reset(iso, boot->ToObject()->CallAsFunction(nodeObject, 0, NULL)->ToObject());
    thread->nodeObject.Reset(iso, nodeObject);
#else
    thread->nodeObject= v8::Persistent<v8::Object>::New(threadTemplate->NewInstance());
    thread->nodeObject->ToObject()->SetPointerInInternalField(0, thread);
    thread->nodeObject->ToObject()->Set(v8::String::NewSymbol("id"), v8::Integer::New(thread->id));
    thread->nodeObject->ToObject()->Set(v8::String::NewSymbol("version"), v8::String::New(k_TAGG_VERSION));
    thread->nodeDispatchEvents= v8::Persistent<v8::Object>::New(boot_js->ToObject()->CallAsFunction(thread->nodeObject->ToObject(), 0, NULL)->ToObject());
#endif

#ifdef TAGG_USE_LIBUV
      uv_async_init(uv_default_loop(), &thread->async_watcher, Callback);
#else
      ev_async_init(&thread->async_watcher, Callback);
      ev_async_start(EV_DEFAULT_UC_ &thread->async_watcher);
      ev_ref(EV_DEFAULT_UC);
#endif

#ifdef TAGG_USE_NEW_API
    args.GetReturnValue().Set(nodeObject);
#else
    return thread->nodeObject;
#endif
}







//Esto es lo primero que llama node al hacer require('threads_a_gogo')
void Init (v8::Handle<v8::Object> target) {
  qitemsStore= nuQueue();
  useLocker= v8::Locker::IsActive();
  
#ifdef TAGG_USE_NEW_API

  v8::Isolate* iso= v8::Isolate::GetCurrent();
  boot_js.Reset(iso, v8::Script::Compile(v8::String::NewFromUtf8(iso, kBoot_js))->Run()->ToObject());
  target->Set(v8::String::NewFromUtf8(iso, "create"), v8::FunctionTemplate::New(iso, Create)->GetFunction());
  target->Set(v8::String::NewFromUtf8(iso, "createPool"), v8::Script::Compile(v8::String::NewFromUtf8(iso, kPool_js))->Run()->ToObject());
  target->Set(v8::String::NewFromUtf8(iso, "version"), v8::String::NewFromUtf8(iso, k_TAGG_VERSION));
  
#else

  boot_js= v8::Persistent<v8::Object>::New(v8::Script::Compile(v8::String::New(kBoot_js))->Run()->ToObject());
  
  threadTemplate= v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New());
  threadTemplate->SetInternalFieldCount(1);
  threadTemplate->Set(v8::String::NewSymbol("load"), v8::FunctionTemplate::New(Load));
  threadTemplate->Set(v8::String::NewSymbol("eval"), v8::FunctionTemplate::New(Eval));
  threadTemplate->Set(v8::String::NewSymbol("emit"), v8::FunctionTemplate::New(processEmit));
  threadTemplate->Set(v8::String::NewSymbol("destroy"), v8::FunctionTemplate::New(Destroy));
  
  target->Set(v8::String::NewSymbol("create"), v8::FunctionTemplate::New(Create)->GetFunction());
  target->Set(v8::String::NewSymbol("createPool"), v8::Script::Compile(v8::String::New(kPool_js))->Run()->ToObject());
  target->Set(v8::String::NewSymbol("version"), v8::String::New(k_TAGG_VERSION));
  
#endif
  
}

NODE_MODULE(threads_a_gogo, Init)

/*
gcc -E -I /Users/jorge/JAVASCRIPT/binarios/include/node -o /o.c /Users/jorge/JAVASCRIPT/threads_a_gogo/src/threads_a_gogo.cc && mate /o.c

tagg=require('threads_a_gogo')
process.versions
t=tagg.create()
*/