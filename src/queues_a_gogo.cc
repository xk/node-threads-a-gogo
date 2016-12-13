//2011-11 Proyectos Equis Ka, s.l., jorge@jorgechamorro.com
//queues_a_gogo.cc

static inline void qPush (typeQueueItem* qitem, typeQueue* queue) {
  qitem->next= NULL;
  pthread_mutex_lock(&queue->mutex);
  queue->last ? queue->last->next= qitem : queue->first= qitem;
  queue->last= qitem;
  pthread_mutex_unlock(&queue->mutex);
}

static inline typeQueueItem* qPull (typeQueue* queue) {
  typeQueueItem* qitem= NULL;
  pthread_mutex_lock(&queue->mutex);
  if ((qitem= queue->first))
    queue->last == qitem ? queue->first= queue->last= NULL : queue->first= qitem->next;
  pthread_mutex_unlock(&queue->mutex);
  return qitem;
}

static inline typeQueueItem* nuQitem () {
  typeQueueItem* qitem= (typeQueueItem*) calloc(1, sizeof(typeQueueItem));
  return qitem;
}

static typeQueue* nuQueue () {
  typeQueue* queue= (typeQueue*) calloc(1, sizeof(typeQueue));
  pthread_mutex_init(&(queue->mutex), NULL);
  return queue;
}
