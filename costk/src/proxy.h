#ifndef __PROXY_INC__
#define __PROXY_INC__

#include "extern.h"

class ProxyPool;
class Manager;

class Proxy : public Runnable {
private:
   uint32_t iofd;
   uint32_t epfd;
   int sordflag;
   int active;
   int inactive;
   int closed;
   int epollTimeout;

   int mSessionId;
   int mInputHold;
   int mOutputHold;
   int mInputAvailable;
   int mOutputAvailable;
   int mNotifyOutputAvailable;
   int mNotifyInputRelease;
   int mSiblingClosed;
   int mSiblingNotifyLimit;
   int mMaxProxyQueueLength;
   int mTimeoutCounter;
   int mActiveTimeout;
   int mInactiveMultiple;
   unsigned int mCount;
   int mUseSplice;
   int mSplicePipe[2];
   int mSplicePending;
   int mSplicePeerFd;
   int mSplicePeerSourceFd;
   int mSpliceChunk;

   struct in_addr address;
   struct in_addr sibling_address;
   in_port_t port;

   CList<Buffer *> queue;

   Manager    *mgr;
   Proxy      *sibling;
   ProxyPool  *pool;
   BufferPool *bpool;

   void handleInput(int fd);
   void handleOutput(int fd);
   int  handleSplice(int fd);
   int  drainSplicePipe(int fd);
   int  initSplice();
   void closeSplice();
   int  ensureSplicePeer();
   void disableSplice(const char *reason);
   void handleHangup(int fd);
   int  clear_recent_ftp(struct in_addr *addr);

protected:
   virtual int run(); 
   virtual void init();

public:
   Proxy(ProxyPool *ppool);
   virtual ~Proxy();

   virtual int init(BufferPool *pool, Proxy *sib, int fd, int sord, struct sockaddr_in& myad, sockaddr_in& sibad, int id);

   void reset();

   int  isActive()             {return active;}
   int  isInactive()           {return inactive;}
   int  isInputHeld()          {return mInputHold;}
   int  getFD()                {return iofd;}
   Manager *getManager()       {return mgr;}
   Manager *setManager(Manager *m) {
      mgr = m;
      return m;
   }

   void setActive(int state)   {active = state;}
   void setInactive(int state) {inactive = state;}
   int  queueOutput(Buffer *item);
   int  notifyClose(); 
   void notifyInputRelease();
   void notifyOutputAvailable();
   void terminate();
   void returnToPool();
   void goInactive();
   void goActive();
};
#endif
