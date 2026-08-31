#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "proxy.h"

static Semaphore clearRecent;

Proxy::Proxy (ProxyPool *ppool) {
   pool = ppool;
   active=FALSE;
   inactive=FALSE;
   clearRecent.V();
   mgr = NULL;
   init();
   start();
}


Proxy::~Proxy() {
}


void
Proxy::init() {
   mInputHold = FALSE;
   mOutputHold = FALSE;
   mInputAvailable = FALSE;
   mOutputAvailable = FALSE;
   mNotifyOutputAvailable = FALSE;
   mNotifyInputRelease = FALSE;
   mSiblingClosed = FALSE;
   mSiblingNotifyLimit = gArgs->get("siblingNotifyLimit",5);
   mMaxProxyQueueLength = gArgs->get("maxProxyQueueLength",100);
   mActiveTimeout = gArgs->get("activeTimeout",EPOLL_ACTIVE_TIMEOUT);
   mInactiveMultiple = gArgs->get("InactiveMultiple",EPOLL_INACTIVE_MULTIPLE);
   mTimeoutCounter = 0;
   mCount = 0;
   mUseSplice = gArgs->get("useSplice", TRUE);
   mSplicePipe[0] = -1;
   mSplicePipe[1] = -1;
   mSplicePending = 0;
   mSplicePeerFd = 0;
   mSplicePeerSourceFd = 0;
   mSpliceChunk = gArgs->get("spliceChunk", 65536);

   iofd = 0;
   epfd = 0;
   port = 0;
   sordflag = 0;
   closed = FALSE;
}


void
Proxy::terminate() {
   stop();

   if (isActive() == FALSE || isInactive() == FALSE) {
      start();
   }
}


void
Proxy::returnToPool() {
   reset();
   pool->poolProxy(this);
}


void 
Proxy::goInactive() {
   pool->setInactive(this);
   epollTimeout = EPOLL_TIMEOUT*4;
   mTimeoutCounter = 0;
}


void 
Proxy::goActive() {
   pool->setActive(this);
   epollTimeout = EPOLL_TIMEOUT;
   mTimeoutCounter = 0;
}


int
Proxy::init(BufferPool *pool, Proxy *sib, int fd, int sord, struct sockaddr_in& myad, sockaddr_in& sibad, int sid) {
   int res=0;

   bpool = pool;
   sibling = sib;
   iofd  = fd;
   address = myad.sin_addr;
   port = myad.sin_port;
   sordflag = sord;
   mSessionId = sid;
   sibling_address = sibad.sin_addr;

   if((res=getEPollFD(0, iofd, EPOLLIN|EPOLLOUT|EPOLLET)) < 0) {
      LG_ERROR(gLog, "PROXY(%s): Cannot create epoll descriptor, terminating session", getName());
   }
   else {
      char buf[20];

      epfd = res;
      if (mUseSplice && initSplice() < 0) {
         disableSplice("pipe setup failed");
      }
      if (mUseSplice == TRUE) {
         struct epoll_event ev;
         ev.events = EPOLL_BASE|EPOLLIN|EPOLLET;
         ev.data.fd = iofd;
         if (epoll_ctl(epfd, EPOLL_CTL_MOD, iofd, &ev) < 0) {
            disableSplice("cannot switch input fd to splice epoll mode");
         }
      }
      start();

      if (sordflag == TRUE) {
         snprintf(buf, 15, "src[%d]", port);
      }
      else {
         snprintf(buf, 15, "dst[%d]", port);
      }

      LG_DEBUG(gLog, "PROXY(%s:%d): Manager has initialized ...", buf, sordflag);
   }

   return res;
}


int 
Proxy::clear_recent_ftp(struct in_addr *addr) {
   char buf[128];
   int res = 0;
   int fd;

   if (addr == NULL) {
      LG_DEBUG(gLog, "PROXY(%s): Cannot clear recent FTP address", getName());
      return -1;
   }

   sprintf(buf, "-%d.%d.%d.%d", 
           (addr->s_addr >> 0)  & 0xff,
           (addr->s_addr >> 8)  & 0xff,
           (addr->s_addr >> 16) & 0xff,
           (addr->s_addr >> 24) & 0xff);

   clearRecent.P();
   if ((fd=open("/proc/net/xt_recent/FTP", O_RDWR)) < 0) {
      clearRecent.V();
      LG_ERROR(gLog, "PROXY(%s): Cannot open /proc/net/xt_recent/FTP, err=%d", getName(), errno);
      return -1;
   }

   res=write(fd,buf,strlen(buf));
   clearRecent.V();
   close(fd);

   if (res <= 0) {
      LG_ERROR(gLog, "PROXY(%s): Cannot write to /proc/net/xt_recent/FTP, err=%d", getName(), errno);
      res = -1;
   }
 
   return res;
}



void
Proxy::reset() {
   closeSplice();

   if (iofd != 0) {
      close(iofd);
   }

   if (epfd != 0) {
      close(epfd);
   }

   for (int i=queue.length(); i > 0; i--) {
      Buffer *buf = queue.front();
      queue.pop_front();
      buf->returnToPool();
   }

   // FIXME: configure via command line parameters
   if (port == htons(21) || port == htons(1224)) {
      clear_recent_ftp(&sibling_address);
   }

   LG_DEBUG(gLog, "PROXY(%s): Proxy reset, going idle", getName());
   init();
}


int
Proxy::run() {
   int  toggle=0;
   char buf[120];

   do {
      // First we wait for us to be initialized with an appropriate set of
      // ports.
      LG_DEBUG(gLog, "PROXY(%s:%d): Waiting for work...", getName(), sordflag);

      waitStart();

      if (isTerminated() == TRUE) {
         LG_DEBUG(gLog, "PROXY(%s:%d): isTerminated() is TRUE ...", getName(), sordflag);
         break;
      }

      sprintf(buf, "%06d:%d", mSessionId, ntohs(port));
      pushName(buf);

      LG_DEBUG(gLog, "PROXY(%s:%d): Working ... iofd=%d", getName(), sordflag, iofd);

      // NOTE: have to use epoll even with single descriptor because the proxy may
      // run in a system with 100's of thousands of open FD's, select is no good in
      // that scenario.  We need to time out on io, otherwise the proxy will not
      // self reap.
      while (closed == FALSE) {
         struct epoll_event event;
	 int    nfds;
         int    flags;
         int    fd;
// FIXME:
//         int    timeout = epollTimeout;
int    timeout = 5;

         // NOTE: really we need to do this by signalling I/O on an fd so that epoll_wait
         //       cannot miss the signal.  For now we reduce the likelihood of a miss by
         //       setting the timeout to 0 if it looks like we were signalled. Or if output
         //       is waiting and there is no output hold
         unblockSignal(SIGUSR1);

         if (mNotifyInputRelease || mNotifyOutputAvailable || (mOutputAvailable && !mOutputHold)) {
            timeout = 0;
         }
         if (mUseSplice == TRUE) {
            ensureSplicePeer();

            if (mSplicePending > 0 || mInputAvailable == TRUE) {
               timeout = 0;
            }
         }

         LG_DEBUG(gLog, "PROXY(%s) Entering EPOLL.... %d",getName(), timeout);
 	 nfds = epoll_wait(epfd, &event, 1, timeout);
         LG_DEBUG(gLog, "PROXY(%s) Exit EPOLL....",getName());

         blockSignal(SIGUSR1);
         LG_DEBUG(gLog, "EPOLL(%s:%d): +++++++++++++++++++ ", getName(), nfds);

         if (nfds > 0) {
	    flags = event.events;
            fd    = event.data.fd;

            LG_DEBUG(gLog, "EPOLL(%s:%d): +++++++++++++++++++ fd=%d flags=0x%04x", getName(), nfds, fd, flags);

            mTimeoutCounter = 0;

	    if (flags & (EPOLLERR|EPOLLHUP)) {
                LG_ERROR(gLog, "PROXY(%s): Premature termination, connection failed.", getName());
		closed = sibling->notifyClose();
		continue;
	    }

            if (fd == iofd && flags & EPOLLIN) {
               mInputAvailable |= TRUE;
            }

            if (flags & EPOLLOUT) {
               mOutputHold = FALSE;
               if (mUseSplice == TRUE && fd == mSplicePeerFd && mSplicePending > 0) {
                  mOutputAvailable = TRUE;
               }
            }
         }
         else {
            // We may have been woken up from epoll using a signal, in this case make sure we
            // use our io file descriptor for io.
            fd = iofd;
         }

         if (mNotifyInputRelease == TRUE) {
             mNotifyInputRelease = FALSE;
             mInputHold = FALSE;
         }

         if (mNotifyOutputAvailable == TRUE) {
             mNotifyOutputAvailable = FALSE;
             mOutputAvailable = TRUE;
         }

         if (mSiblingClosed == TRUE) {
             mInputHold = TRUE;

             if (mOutputAvailable == FALSE && mSplicePending == 0) {
                closed = TRUE;
                break;
             }
         }

         LG_DEBUG(gLog, "EPOLL(%s:%d): +++++++++++++++++++ fd=%d IA=%d OA=%d IH=%d OH=%d SC=%d", getName(), nfds, fd,
                     mInputAvailable,mOutputAvailable,mInputHold,mOutputHold,mSiblingClosed);

         if (mInputAvailable || mOutputAvailable || mSplicePending > 0) {
            goActive();

            if (mSiblingClosed == FALSE) {
               sibling->goActive();
            }
         }
         else if (nfds == 0 && timeout != 0) {
            mTimeoutCounter++;

            if (isActive() == true) {
               if (mTimeoutCounter*timeout/1000 > mActiveTimeout) {
                  goInactive();

                  if (mSiblingClosed == FALSE) {
                     sibling->goInactive();
                  }
                  LG_DEBUG(gLog, "PROXY(%s): Goes inactive...", getName());
               }
            }
            else if (mTimeoutCounter*timeout/1000 > (mActiveTimeout*mInactiveMultiple)) {
               LG_DEBUG(gLog, "PROXY(%s): Timed out, closing...", getName());
               closed = sibling->notifyClose();
            }

            continue;
         }

         if (mUseSplice == TRUE) {
            if (handleSplice(fd) < 0) {
               if (toggle == 1) {
                  handleInput(fd);
                  if (closed == FALSE) {
                     handleOutput(fd);
                  }
               }
               else {
                  if (closed == FALSE) {
                     handleOutput(fd);
                  }
                  handleInput(fd);
               }
            }
         }
         else {
            if (toggle == 1) {
               handleInput(fd);
               if (closed == FALSE) {
                  handleOutput(fd);
               }
            }
            else {
               if (closed == FALSE) {
                  handleOutput(fd);
               }
               handleInput(fd);
            }
         }

         if (queue.length() < mMaxProxyQueueLength && sibling->isInputHeld()) {
            sibling->notifyInputRelease();
         }

         //toggle = 1-toggle;
      }

      // Note returnToPoll() closes the FD's by calling reset() on the proxy.
      LG_DEBUG(gLog, "PROXY(%s): MISSION ACCOMPLISHED", getName());
      popName();
      returnToPool();
   }
   while(isTerminated() == FALSE);

   LG_DEBUG(gLog, "PROXY(%s): Terminating ...", getName());
   return(0);
}


int
Proxy::initSplice() {
   if (mSplicePipe[0] >= 0 && mSplicePipe[1] >= 0) {
      return 0;
   }

   if (pipe(mSplicePipe) < 0) {
      LG_ERROR(gLog, "PROXY(%s): Cannot create splice pipe, err=%d", getName(), errno);
      mSplicePipe[0] = -1;
      mSplicePipe[1] = -1;
      return -1;
   }

   for (int i=0; i<2; i++) {
      int flags = fcntl(mSplicePipe[i], F_GETFL);
      if (flags < 0 || fcntl(mSplicePipe[i], F_SETFL, flags|O_NONBLOCK) < 0) {
         LG_ERROR(gLog, "PROXY(%s): Cannot set splice pipe nonblock, err=%d", getName(), errno);
         closeSplice();
         return -1;
      }
   }

#ifdef F_SETPIPE_SZ
   if (mSpliceChunk > 0) {
      fcntl(mSplicePipe[0], F_SETPIPE_SZ, mSpliceChunk);
   }
#endif

   return 0;
}


void
Proxy::closeSplice() {
   if (mSplicePipe[0] >= 0) {
      close(mSplicePipe[0]);
   }
   if (mSplicePipe[1] >= 0) {
      close(mSplicePipe[1]);
   }
   if (mSplicePeerFd > 0) {
      close(mSplicePeerFd);
   }

   mSplicePipe[0] = -1;
   mSplicePipe[1] = -1;
   mSplicePending = 0;
   mSplicePeerFd = 0;
   mSplicePeerSourceFd = 0;
}


void
Proxy::disableSplice(const char *reason) {
   if (mUseSplice == TRUE) {
      LG_WARNING(gLog, "PROXY(%s): Disabling splice path: %s", getName(), (reason == NULL) ? "unknown" : reason);
   }

   closeSplice();
   mUseSplice = FALSE;
   mOutputAvailable = FALSE;
   mOutputHold = FALSE;

   if (epfd > 0 && iofd > 0) {
      struct epoll_event ev;
      ev.events = EPOLL_BASE|EPOLLIN|EPOLLOUT|EPOLLET;
      ev.data.fd = iofd;
      epoll_ctl(epfd, EPOLL_CTL_MOD, iofd, &ev);
   }
}


int
Proxy::ensureSplicePeer() {
   struct epoll_event ev;
   int peerfd;
   int dupfd;

   if (mUseSplice == FALSE || sibling == NULL || epfd <= 0) {
      return -1;
   }

   peerfd = sibling->getFD();
   if (peerfd <= 0) {
      return -1;
   }

   if (peerfd == mSplicePeerSourceFd && mSplicePeerFd > 0) {
      return 0;
   }

   if (mSplicePeerFd > 0) {
      epoll_ctl(epfd, EPOLL_CTL_DEL, mSplicePeerFd, NULL);
      close(mSplicePeerFd);
      mSplicePeerFd = 0;
      mSplicePeerSourceFd = 0;
   }

   dupfd = dup(peerfd);
   if (dupfd < 0) {
      LG_ERROR(gLog, "PROXY(%s): Cannot duplicate splice peer fd %d, err=%d", getName(), peerfd, errno);
      return -1;
   }

   int flags = fcntl(dupfd, F_GETFL);
   if (flags < 0 || fcntl(dupfd, F_SETFL, flags|O_NONBLOCK) < 0) {
      LG_ERROR(gLog, "PROXY(%s): Cannot set splice peer fd %d nonblock, err=%d", getName(), dupfd, errno);
      close(dupfd);
      return -1;
   }

   ev.events = EPOLL_BASE|EPOLLOUT|EPOLLET;
   ev.data.fd = dupfd;
   if (epoll_ctl(epfd, EPOLL_CTL_ADD, dupfd, &ev) < 0) {
      if (errno == EEXIST && epoll_ctl(epfd, EPOLL_CTL_MOD, dupfd, &ev) == 0) {
         mSplicePeerFd = dupfd;
         mSplicePeerSourceFd = peerfd;
         return 0;
      }

      LG_ERROR(gLog, "PROXY(%s): Cannot add splice peer fd %d to epoll, err=%d", getName(), dupfd, errno);
      close(dupfd);
      return -1;
   }

   mSplicePeerFd = dupfd;
   mSplicePeerSourceFd = peerfd;
   return 0;
}


int
Proxy::drainSplicePipe(int fd) {
   int count = 0;

   while (mSplicePending > 0 && count < 200) {
      ssize_t len;

      errno = 0;
      len = splice(mSplicePipe[0], NULL, fd, NULL, mSplicePending, SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
      count++;
      LG_DEBUG(gLog, "PROXY(%s): fd=%d Splice pipe ========> %d %d", getName(), fd, (int)len, errno);

      if (len > 0) {
         mSplicePending -= len;
         continue;
      }

      if (len < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
         mOutputHold = TRUE;
         mOutputAvailable = TRUE;
         return 0;
      }

      closed = sibling->notifyClose();
      return -1;
   }

   if (mSplicePending == 0) {
      mOutputAvailable = FALSE;
      closed |= mSiblingClosed;
   }

   return 0;
}


int
Proxy::handleSplice(int fd) {
   int peerfd;
   int count = 0;

   if (mUseSplice == FALSE) {
      return -1;
   }

   if (initSplice() < 0 || ensureSplicePeer() < 0) {
      return 0;
   }

   peerfd = mSplicePeerFd;

   if (mSplicePending > 0 && mOutputHold == FALSE) {
      if (drainSplicePipe(peerfd) < 0) {
         return 0;
      }
   }

   while (closed == FALSE && mInputAvailable == TRUE && mInputHold == FALSE && mSplicePending == 0 && count < 200) {
      ssize_t len;
      size_t chunk = (mSpliceChunk > 0) ? (size_t)mSpliceChunk : (size_t)65536;

      errno = 0;
      len = splice(iofd, NULL, mSplicePipe[1], NULL, chunk, SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
      count++;
      LG_DEBUG(gLog, "PROXY(%s): fd=%d Splice <======== %d %d", getName(), iofd, (int)len, errno);

      if (len > 0) {
         mSplicePending += len;
         mOutputAvailable = TRUE;
         if (mOutputHold == FALSE && drainSplicePipe(peerfd) < 0) {
            return 0;
         }
         continue;
      }

      if (len == 0) {
         mInputAvailable = FALSE;
         closed = sibling->notifyClose();
         mInputHold = TRUE;
         return 0;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {
         mInputAvailable = FALSE;
         return 0;
      }

      if (errno == EINVAL || errno == ENOSYS || errno == EBADF) {
         disableSplice("kernel/socket does not support splice");
         return -1;
      }

      closed = sibling->notifyClose();
      mInputHold = TRUE;
      return 0;
   }

   return 0;
}


void
Proxy::handleInput(int fd) {
   if (mInputHold == FALSE) {
       while(mInputAvailable == TRUE && mInputHold == FALSE) {
	  Buffer *buf;
	  int len;

          buf = bpool->getBuffer();

	  errno = 0;
	  len=read(iofd, buf->data, PACKET_LEN);
	  LG_DEBUG(gLog, "PROXY(%s): fd=%d Read <======== %d %d",getName(),fd,len,errno);
	 
	  if (len <= 0) {
	     buf->returnToPool();
	     mInputAvailable = FALSE;
/*
             if (errno == EAGAIN) {
		mInputHold = TRUE;
             }
*/
	     if (len != -1 || errno != EAGAIN) {
	        LG_DEBUG(gLog, "PROXY(%s): fd=%d Read <======== %d %d NOTIFY SIBLING OF CLOSING",getName(),fd,len,errno);
		closed = sibling->notifyClose();
		mInputHold = TRUE;
	     }
	  }
	  else {
	     buf->offset = 0;
	     buf->length = len;
	     mInputHold = sibling->queueOutput(buf);
	  }
       }
   }
}


void
Proxy::handleOutput(int fd) {
   if (mOutputHold == FALSE) {
      int count = 0;
      Buffer *buf = NULL;

      if (queue.isEmpty() == FALSE) {
         buf = queue.front();
      }

      while(buf != NULL && count < 200 && mOutputAvailable == TRUE && mOutputHold == FALSE) {
	 int len;

         errno = 0;
	 len = write(fd, buf->data+buf->offset, buf->length);
         count++;
	 LG_DEBUG(gLog, "PROXY(%s): fd=%d Write ========> %d %d",getName(),fd,len,errno);

	 if (len <= 0) {
	    mOutputHold = TRUE;

	    if (len != 0 && errno != EAGAIN) {
	       closed = sibling->notifyClose();
	    }
	 }
	 else if (len < buf->length) {
	    LG_DEBUG(gLog, "PROXY(%s): fd=%d Partial write %d",getName(),fd,len);
	    buf->offset += len;
	    buf->length -= len;
	 }
	 else {
            int size;

	    queue.pop_front();
	    buf->returnToPool();

            if (queue.isEmpty() == FALSE) {
               buf = queue.front();
               size = queue.length();
            }
            else {
               size = 0;
               buf = NULL;
            }

	    LG_DEBUG(gLog, "PROXY(%s): fd=%d Full write %d, qlen=%d",getName(),fd,len,size);

            if (size < mSiblingNotifyLimit && sibling->isInputHeld()) {
                sibling->notifyInputRelease();
            }
	 }
      }

      if (buf == NULL) {
	 mOutputAvailable = FALSE;
	 closed |= mSiblingClosed;
      }
   }
}


void
Proxy::handleHangup(int fd) {
   LG_DEBUG(gLog, "PROXY(%s): Hangup ...", getName());

   mInputHold = TRUE;
   mOutputHold = TRUE;
  
   sibling->notifyClose();
   closed = TRUE;
}


int 
Proxy::notifyClose() {
   if (closed == FALSE) {
      mInputHold = TRUE;
      mSiblingClosed = TRUE;
      signal(SIGUSR1);
      LG_DEBUG(gLog, "PROXY(%s): Sibling has just notified that it is closed", getName());
   }

   return(TRUE);
}


void
Proxy::notifyInputRelease() {
   if (closed == FALSE) {
      mNotifyInputRelease = TRUE;
      signal(SIGUSR1);
   }
}


void
Proxy::notifyOutputAvailable() {
   if (closed == FALSE) {
      mNotifyOutputAvailable = TRUE;
      signal(SIGUSR1);
   }
}


int 
Proxy::queueOutput(Buffer *item) {
   if (closed == FALSE) {
      queue.push_back(item);

      if(mOutputAvailable == FALSE) {
	 notifyOutputAvailable();
      }

      mCount++;
      if (mCount % 50 == 0)
          sched_yield();
      LG_DEBUG(gLog, "PROXY(%s): Enqueued output buffer %d", getName(), queue.length());
   }

   return (closed | queue.length() > mMaxProxyQueueLength - 10);
}
