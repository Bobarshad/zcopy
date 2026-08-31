#include "extern.h"

BufferPool::BufferPool (int min, int max, int port) {
   char tname[20];

   if (min > max || min < 1) {
      min = DEFAULT_BFPOOL_MIN_SIZE;

      if (min > max) {
         max = DEFAULT_BFPOOL_MAX_SIZE;
      }
   }

   minSize = min;
   maxSize = max;

   bufferPoolReapInterval = gArgs->get("bufferPoolReapInterval", DEFAULT_BFPOOL_REAP_INTERVAL);

   for (int i=0; i<min; i++) {
      poolBuffer(new Buffer(this));
   } 

   LG_DEBUG(gLog, "BP: Create buffer pool for port %d, min=%d, max=%d", port, min, max);
   sprintf(tname, "BP(%d)", port);
   setName(tname);
}


BufferPool::~BufferPool() {
   if (pool.length() > 0) {
      trim(0);
   }
}


Buffer *
BufferPool::getBuffer() {
   Buffer *item = NULL;

   get.P();
   if (pool.isEmpty() == FALSE) {
      item = pool.front();
      pool.pop_front();
   }
   get.V();

   if (item == NULL) {
      item = new Buffer(this);
   }

   return item;
}


void
BufferPool::poolBuffer(Buffer *item) {
   if (item == NULL) {
      return;
   }

   if (pool.length() < maxSize) {
      pool.push_back(item);
   }
   else {
      delete item;
   }
}


// FIXME: Kept for posterity, it is not necessary to run a reaper in the background
//        for buffer pools.  We do keep them as threads as in the future we may need
//        some additional smarts in the process so haven't totally thrown this out.
//
//        To re-enable reaping, just call start() in the constructor.
//
int
BufferPool::run() {
   do {
      if (pool.length() > maxSize) {
         trim(maxSize);
      }

      sleep(bufferPoolReapInterval);
   }
   while (isTerminated() == FALSE);

   return 0;
}


void
BufferPool::trim(int len) {
   while (pool.length() > len) {
      Buffer *item = NULL;

      get.P();
      if (pool.isEmpty() == FALSE) {
         item = pool.front();
         pool.pop_front();
      }
      get.V();

      if (item != NULL) {
         delete item;
      }
   }
}
