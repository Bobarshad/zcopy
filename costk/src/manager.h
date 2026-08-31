#ifndef __MANAGER__INC__
#define __MANAGER__INC__

#include "extern.h"

class Manager : public Runnable {
private:
   int         lsock;
   int         lport;
   int         terminate;
   ProxyPool  *mProxyPool;
   BufferPool *mBufferPool;
   in_addr_t   mHeartbeatIP;

   Semaphore   tallys;
   int         mCompleted;
   int         mActive;
   int         mInactive;
   time_t      mWhen;

protected:
   virtual int  init(int port);
   virtual int  run();

public:
   Manager(int lport, ProxyPool *proxyPool);
   virtual ~Manager();
   int getPort()   {return lport;}
   int getSocket() {return lsock;}

   char *getTotals(char *buf, int len);

   void adjActive(int amt);
   void adjInactive(int amt);
   void adjCompleted(int amt);
};
#endif
