#include "extern.h"

Manager::Manager(int port, ProxyPool *pool) {
   int res;
   char tname[20];

   lsock = 0;
   lport = port;
   terminate = FALSE;
   
   sprintf(tname, "MGR(%d)", port);
   setName(tname);

   // do this before call to init, as the manager is halted until init is called.
   mProxyPool = pool;

   // FIXME: should be configurable (min,max)
   mBufferPool = new BufferPool(100,1000,port);

   // set the watchdog IP
   mHeartbeatIP = htonl(siptoaddr(gArgs->get("heartBeatIP", "")));

   mWhen = time(NULL);
   mActive = 0;
   mInactive = 0;
   mCompleted = 0;

   LG_DEBUG(gLog, "Initialize Manager Service for Port(%d)", port);

   if ((res=init(port)) < 0) {
      LG_ERROR(gLog, "Manager Service for Port(%d) failed to initialize, err=%d", port, res);
   }
   else {
      start();
   }
}


Manager::~Manager() {
   LG_DEBUG(gLog, "Shutdown Manager Service for Port(%d)", lport);
   terminate = TRUE;

   if (lsock != 0) {
      close(lsock);
   }

   delete mBufferPool;
}


// If we get here, then all prerequisites have been setup.
// First create the epoll socket, add the listening socket to it, and then wait for connections.
//
// When we get a connection accept, add it to the epoll descriptor, get a worker, and 
// hand the worker the events on the FD, use a single lookuptable to lookup workers for specific
// file descriptors.
//
int 
Manager::run() {
   int sessionid = 0;
   struct sockaddr_in srcaddr;
   struct sockaddr_in dstaddr;
   socklen_t saddrlen = sizeof(srcaddr);
   socklen_t daddrlen = sizeof(dstaddr);

   if (lsock == 0) {
      return(-1);
   }

   LG_DEBUG(gLog, "MGR(%d): Startup", lport);
   nice(-1);

   do {
      int srcfd;
      int dstfd;
      int res;

      LG_DEBUG(gLog, "MGR(%d): Waiting for new connection ...", lport);

      if (sessionid > 999999) {
         sessionid = 0;
      }

      if ((srcfd=accept(lsock, (struct sockaddr *) &srcaddr, &saddrlen)) < 0) {
         LG_ERROR(gLog, "MGR(%d): accept error [%s]",lport,strerror(errno));
	 continue;
      }

      if (terminate == TRUE) {
         break;
      }
      
      if (mHeartbeatIP != 0 && mHeartbeatIP == srcaddr.sin_addr.s_addr) {
         close(srcfd);
         LG_DEBUG(gLog, "MGR(%d): Heartbeat ping, closed session",lport);
         continue;
      }

      getsockname(srcfd, (struct sockaddr *) &dstaddr, &daddrlen);

      LG_NOTICE(gLog, "MGR(%d): Accept new session(%d) %d.%d.%d.%d:%d => %d.%d.%d.%d:%d", lport, sessionid, 
		 (srcaddr.sin_addr.s_addr >> 0)  & 0xff,
		 (srcaddr.sin_addr.s_addr >> 8)  & 0xff,
		 (srcaddr.sin_addr.s_addr >> 16) & 0xff,
		 (srcaddr.sin_addr.s_addr >> 24) & 0xff,
		 ntohs(srcaddr.sin_port),
		 (dstaddr.sin_addr.s_addr >> 0)  & 0xff,
		 (dstaddr.sin_addr.s_addr >> 8)  & 0xff,
		 (dstaddr.sin_addr.s_addr >> 16) & 0xff,
		 (dstaddr.sin_addr.s_addr >> 24) & 0xff,
		 ntohs(dstaddr.sin_port));

      // Get a file descriptor for the other end of the connection
      // Then get us two proxies, one for the upstream traffic, and one for the
      // downstream traffic.  Each is in its own thread, so there is maximum parallelism
      // possible when both ends are transmitting.
      
      struct sockaddr_in sockaddr_temp;
      memcpy(&sockaddr_temp, &srcaddr, sizeof(sockaddr_in));
      sockaddr_temp.sin_addr.s_addr = htonl(INADDR_ANY);
      if ((dstfd=openSocket(/*&srcaddr.sin_addr*/&sockaddr_temp.sin_addr, srcaddr.sin_port, TRUE, TRUE)) > 0) {
	 Proxy *psrc = NULL;
	 Proxy *pdst = NULL;
         int ccfd = srcfd;
	 int res;

         // FIXME: hack hack, need to set CC srcfd if not eth0, otherwise on dstfd, for active FTP, it's the
         // only inbound on port 20, and it's on the inet side.
         if (ntohs(srcaddr.sin_port) == 20) {
            ccfd = dstfd;
         }

         // FIXME: figure this out
         if ((res=set_congestion(ccfd, cca)) < 0) {
	    LG_DEBUG(gLog, "MGR(%d): failed to set WTCP on GGSN socket, err=%d", lport, res);
         }

#if 0
// disabled for now as it doesn't appear to be any need for doing this
         if (ccfd == srcfd) {
            // srcfd on eth1
            bind_to_interface(dstfd, "eth0");
            bind_to_interface(srcfd, "eth1");
         }
         else {
            bind_to_interface(dstfd, "eth1");
            bind_to_interface(srcfd, "eth0");
            // srcfd on eth0
         }
#endif

	 if ((res=fcntl(srcfd, F_SETFL, O_NONBLOCK)) < 0) {
	    LG_ERROR(gLog, "MGR(%d): failed to set NOBLOCK on source socket [%s]", lport,strerror(errno));
	 }
	 else if ((res=connect(dstfd, (struct sockaddr *) &dstaddr, sizeof(dstaddr))) < 0 && errno != EINPROGRESS) {
	    LG_ERROR(gLog, "MGR(%d): error in connecting to [%d.%d.%d.%d:%d] %d %d [%s]", lport,
		       (dstaddr.sin_addr.s_addr >> 0)  & 0xff,
		       (dstaddr.sin_addr.s_addr >> 8)  & 0xff,
		       (dstaddr.sin_addr.s_addr >> 16) & 0xff,
		       (dstaddr.sin_addr.s_addr >> 24) & 0xff,
		       ntohs(dstaddr.sin_port), res, errno, strerror(errno));
	 }
	 else {
	    psrc = mProxyPool->getProxy(this);
	    pdst = mProxyPool->getProxy(this);

	    if (psrc != NULL && pdst != NULL) {
	       if ((res=pdst->init(mBufferPool, psrc, dstfd, FALSE, dstaddr, srcaddr, sessionid)) < 0) {
		  LG_ERROR(gLog, "MGR(%d): failed to initialize destination proxy %d", lport, res);
	       }
	       else if ((res=psrc->init(mBufferPool, pdst, srcfd, FALSE, srcaddr, dstaddr, sessionid)) < 0) {
		  LG_ERROR(gLog, "MGR(%d): failed to initialize source proxy %d", lport, res);
	       }
	    }
	    LG_NOTICE(gLog, "MGR(%d): in connecting to [%d.%d.%d.%d:%d] %d %d [%s]", lport,
		       (dstaddr.sin_addr.s_addr >> 0)  & 0xff,
		       (dstaddr.sin_addr.s_addr >> 8)  & 0xff,
		       (dstaddr.sin_addr.s_addr >> 16) & 0xff,
		       (dstaddr.sin_addr.s_addr >> 24) & 0xff,
		       ntohs(dstaddr.sin_port), res, errno, strerror(errno));
	 }

	 if (res < 0 || psrc == NULL || pdst == NULL) {
	    LG_DEBUG(gLog, "MGR(%d): Failed to initialize proxies, closing session. res=%d", lport, res);

	    if (psrc != NULL) {
	       psrc->returnToPool();
	    }
	    if (pdst != NULL) {
	       pdst->returnToPool();
	    }

	    close(srcfd);
	    close(dstfd);
	 }

         sessionid++;
	 LG_NOTICE(gLog, 
                  "MGR(%d): open destination socket to %d.%d.%d.%d:%d res=%d.\n", lport,
		  (dstaddr.sin_addr.s_addr >> 0)  & 0xff,
		  (dstaddr.sin_addr.s_addr >> 8)  & 0xff,
		  (dstaddr.sin_addr.s_addr >> 16) & 0xff,
		  (dstaddr.sin_addr.s_addr >> 24) & 0xff,
		  ntohs(dstaddr.sin_port), dstfd);
      }
      else {
	 LG_ERROR(gLog, 
                  "MGR(%d): Failed to open destination socket to %d.%d.%d.%d:%d res=%d.\n", lport,
		  (dstaddr.sin_addr.s_addr >> 0)  & 0xff,
		  (dstaddr.sin_addr.s_addr >> 8)  & 0xff,
		  (dstaddr.sin_addr.s_addr >> 16) & 0xff,
		  (dstaddr.sin_addr.s_addr >> 24) & 0xff,
		  ntohs(dstaddr.sin_port), dstfd);

	 close(srcfd);
      }
   }
   while (terminate == FALSE);

   return(0);
}


int 
Manager::init(int port) {
   struct in_addr addr;
   int flag = 1;
   int res = 0;

   lsock = 0;

   if (port < 1 || port > 65535) {
      return(NET_INV_LPORT);
   }

   addr.s_addr = htonl(INADDR_ANY);
   if ((res=openSocket(&addr, htons(port), TRUE, FALSE)) > 0) {
      LG_NOTICE(gLog, "MGR: listen on %d", port);
      lsock = res;

      if ((res=listen(lsock, 1024)) < 0) {
         res = NET_NO_LISTEN;
	 close(lsock);
         lsock = 0;
      }
   }

   return res;
}


void
Manager::adjCompleted(int amt) {
   tallys.P();
   mCompleted += amt;
   tallys.V();
}


void
Manager::adjActive(int amt) {
   tallys.P();
   mActive += amt;
   tallys.V();
}


void
Manager::adjInactive(int amt) {
   tallys.P();
   mInactive += amt;
   tallys.V();
}


char *
Manager::getTotals(char *buf, int len) {
   time_t now = time(NULL);
   time_t when;
   int done;
   int active;
   int inactive;

   tallys.P();
   when = mWhen;
   done = mCompleted/2;
   active = mActive/2;
   inactive = mInactive/2;
   mCompleted = 0;
   mWhen = now;
   tallys.V();

   snprintf(buf, len-1, "%d D:%ld C:%d A:%d I:%d",
            lport, now-when, done, active, inactive);
   buf[len-1] = '\0';
   return buf;
}
