#include "extern.h"

ProxyPool::ProxyPool(int min, int max) {
   char tname[20];

   if (min > max || min < 1) {
      min = DEFAULT_PPOOL_MIN_SIZE;

      if (min > max) {
         max = DEFAULT_PPOOL_MAX_SIZE;
      }
   }

   minSize = min;
   maxSize = max;

   proxyPoolReapInterval = gArgs->get("proxyPoolReapInterval", DEFAULT_PPOOL_REAP_INTERVAL);

   for (int i=0; i<min; i++) {
      poolProxy(new Proxy(this));
   }

   LG_DEBUG(gLog, "PPOOL: Create proxy pool... min=%d, max=%d", min, max);
   sprintf(tname, "PP(%d,%d)", min, max);
   setName(tname);
   start();
}


ProxyPool::~ProxyPool() {
   if (pool.length() > 0) {
      trim(0);
   }
}


Proxy *
ProxyPool::getProxy(Manager *mgr) {
   Proxy *item = NULL;

   if (isTerminated() == TRUE) {
      return NULL;
   }

   get.P();
   if (pool.isEmpty() == FALSE) {
      item = pool.front();
      pool.pop_front();
   }
   get.V();

   if (item == NULL) {
      item = new Proxy(this);
      LG_DEBUG(gLog, "PPOOL(%s): Create new proxy", item->getName());
   }

   item->setManager(mgr);
   return setActive(item);
}


Proxy *
ProxyPool::poolProxy(Proxy *item) {
   Manager *mgr;

   if (item == NULL) {
      return NULL;
   }

   mgr = item->getManager();

   if (item->isInactive()) {
      inactive.remove(item);
      item->setInactive(FALSE);

      if (mgr != NULL) {
         mgr->adjInactive(-1);
      }
   }
   else if (item->isActive()) {
      active.remove(item);
      item->setActive(FALSE);

      if (mgr != NULL) {
         mgr->adjActive(-1);
      }
   }

   pool.push_back(item);

   if (mgr != NULL) {
      mgr->adjCompleted(1);
   }

   LG_DEBUG(gLog, "POOL: Proxy(%s) Returned to the pool", item->getName());

   return item;
}


Proxy * 
ProxyPool::setActive(Proxy *item) {
   if (item == NULL) {
      return NULL;
   }

   if (item->isActive()) {
      return item;
   }

   if (item->isInactive()) {
      inactive.remove(item);
      item->getManager()->adjInactive(-1);
   }

   active.push_back(item);
   item->setActive(TRUE);
   item->getManager()->adjActive(1);

   LG_DEBUG(gLog, "POOL: Proxy(%s) Set as active", item->getName());

   return item;
}


Proxy * 
ProxyPool::setInactive(Proxy *item) {
   if (item == NULL) {
      return NULL;
   }

   if (item->isInactive()) {
      return item;
   }

   if (item->isActive()) {
      active.remove(item);
      item->setActive(FALSE);
      item->getManager()->adjActive(-1);
   }

   inactive.push_back(item);
   item->setInactive(TRUE);
   item->getManager()->adjInactive(1);

   LG_DEBUG(gLog, "POOL: Proxy(%s) Set as IN active", item->getName());

   return item;
}


int
ProxyPool::run() {
   do {
      if (pool.length() > maxSize) {
          trim(maxSize);
      }
     
      sleep(proxyPoolReapInterval);
      LG_DEBUG(gLog, "POOL: Reaper is running max=%d, length=%d", maxSize, pool.length());
   }
   while (isTerminated() == FALSE);

   return 0;
}


void 
ProxyPool::trim(int len) {
   while(pool.length() > len) {
      Proxy *item = NULL;

      get.P();
      if (pool.isEmpty() == FALSE) {
         item = pool.front();
         pool.pop_front();
      }
      get.V();
 
      if (item != NULL) {
         item->cancel();
         delete item;
      }
   }
}


void
ProxyPool::end() {
}
