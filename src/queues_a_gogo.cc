//2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//queues_a_gogo.cc

static void qPush (typeQueueItem* qitem, typeQueue* queue) {
  qitem->next= NULL;
  
  pthread_mutex_lock(&queue->queueLock);
  if (queue->last) {
    queue->last->next= qitem;
  }
  else {
    queue->first= qitem;
  }
  queue->last= qitem;
  pthread_mutex_unlock(&queue->queueLock);
}

static typeQueueItem* qPull (typeQueue* queue) {

  typeQueueItem* qitem= NULL;
  
  if (queue->first) {
    pthread_mutex_lock(&queue->queueLock);
    qitem= queue->first;
    if (qitem) {
      if (queue->last == qitem)
        queue->first= queue->last= NULL;
      else
        queue->first= qitem->next;
      qitem->next= NULL;
    }
    pthread_mutex_unlock(&queue->queueLock);
  }
  
  return qitem;
}

static typeQueueItem* nuQitem () {
  typeQueueItem* qitem= (typeQueueItem*) calloc(1, sizeof(typeQueueItem));
  return qitem;
}

static typeQueue* nuQueue () {
  typeQueue* queue= (typeQueue*) calloc(1, sizeof(typeQueue));
  pthread_mutex_init(&(queue->queueLock), NULL);
  return queue;
}
