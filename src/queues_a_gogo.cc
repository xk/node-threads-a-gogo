//2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//queues_a_gogo.cc

enum types {
  kItemTypeNONE,
  kItemTypeNumber,
  kItemTypePointer,
  kItemTypeQUIT
};

struct typeQueueItem {
  int itemType;
  typeQueueItem* next;
  union {
    void* asPtr;
    double asNumber;
  };
};

struct typeQueue {
  long int id;
  typeQueueItem* last;
  typeQueueItem* first;
  volatile long int length;
  pthread_mutex_t queueLock;
};

//static typeQueue* queuesPool= NULL;
static typeQueue* freeItemsQueue= NULL;

static void queue_push (typeQueueItem* qitem, typeQueue* queue) {
  qitem->next= NULL;
  
  pthread_mutex_lock(&queue->queueLock);
  if (queue->last) {
    queue->last->next= qitem;
  }
  else {
    queue->first= qitem;
  }
  queue->length+= 1;
  queue->last= qitem;
  pthread_mutex_unlock(&queue->queueLock);
}

static typeQueueItem* queue_pull (typeQueue* queue) {

  typeQueueItem* qitem= NULL;
  
  if (queue->first) {
    pthread_mutex_lock(&queue->queueLock);
    qitem= queue->first;
    if (qitem) {
      if (queue->last == qitem)
        queue->first= queue->last= NULL;
      else
        queue->first= qitem->next;
      queue->length-= 1;
      qitem->next= NULL;
    }
    pthread_mutex_unlock(&queue->queueLock);
  }
  
  return qitem;
}

static typeQueueItem* nuItem (int itemType, void* item) {
  
  typeQueueItem* qitem= queue_pull(freeItemsQueue);
  if (!qitem) qitem= (typeQueueItem*) malloc(sizeof(typeQueueItem));
  qitem->next= NULL;
  qitem->itemType= itemType;
  if (itemType == kItemTypeNumber)
    qitem->asNumber= *((double*) item);
  else if (itemType == kItemTypePointer)
    qitem->asPtr= item;
  
  return qitem;
}

static void destroyItem (typeQueueItem* qitem) {
  if (freeItemsQueue)
    queue_push(qitem, freeItemsQueue);
  else
    free(qitem);
}

static typeQueue* nuQueue (long int id) {

  typeQueue* queue= (typeQueue*) malloc(sizeof(typeQueue));
  queue->id= id;
  queue->length= 0;
  queue->first= queue->last= NULL;
  pthread_mutex_init(&queue->queueLock, NULL);
  
  return queue;
}

/*

static void destroyQueue (typeQueue* queue) {
  if (queuesPool) {
    queue_push(nuItem(kItemTypePointer, queue), queuesPool);
  }
  else {
    free(queue);
  }
}

*/

static void initQueues (void) {
  freeItemsQueue= nuQueue(-2);  //MUST be created before queuesPool
  //queuesPool= nuQueue(-1);
}
