#ifndef __PPOOL_INC__
#define __PPOOL_INC__

#include "extern.h"
#include <list>

class Proxy;
class Manager;

#define DEFAULT_PPOOL_REAP_INTERVAL 10
#define DEFAULT_PPOOL_MIN_SIZE      100
#define DEFAULT_PPOOL_MAX_SIZE      10000

class ProxyPool : public Runnable {
private:
   int maxSize;
   int minSize;

   int proxyPoolReapInterval;
   CList<Proxy *> pool;
   CList<Proxy *> active;
   CList<Proxy *> inactive;
   Semaphore get;

protected:
   virtual int  run();
   virtual void end();
   virtual void trim(int len);

public:
   ProxyPool(int min, int max);
   virtual ~ProxyPool();

   Proxy  *getProxy(Manager *mgr);

   Proxy *poolProxy(Proxy *item);
   Proxy *setActive(Proxy *item);
   Proxy *setInactive(Proxy *item);
};
#endif
