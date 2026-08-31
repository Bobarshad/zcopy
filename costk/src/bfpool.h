#ifndef __BPOOL_INC__
#define __BPOOL_INC__

#include "extern.h"
#include "clist.h"

#define DEFAULT_BFPOOL_REAP_INTERVAL 60
#define DEFAULT_BFPOOL_MIN_SIZE      10
#define DEFAULT_BFPOOL_MAX_SIZE      1000

class BufferPool : public Runnable {
private:
   int minSize;
   int maxSize;
   int bufferPoolReapInterval;
   CList<Buffer *> pool;
   Semaphore get;

   void trim(int len);

protected:
   int run();
   int end();

public:
   BufferPool(int minSize, int maxSize, int port);
   virtual ~BufferPool();

   // Poorman's substitute for synchronized methods
   Buffer  *getBuffer();
   void    poolBuffer(Buffer *item);
};
#endif
