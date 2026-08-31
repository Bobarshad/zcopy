#include "extern.h"

#define EOS(S) ((S) == NULL || *(S) == '\0')

int
openSocket(struct in_addr *addr, in_port_t port, int transparent, int noblock)
{
   struct sockaddr_in saddr;
   int flag = 1;
   int res=0;
   int fd;

   if (addr == NULL) {
      return -1;
   }

   LG_NOTICE(gLog, 
            "Opening local socket for TRANS=%d, NBLCK=%d, %d.%d.%d.%d:%d", transparent, noblock,
	    (addr->s_addr >> 0)  & 0xff,
	    (addr->s_addr >> 8)  & 0xff,
	    (addr->s_addr >> 16) & 0xff,
	    (addr->s_addr >> 24) & 0xff,
	    ntohs(port));

   // we need to bind our src address to the outbound socket.
   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_family      = AF_INET;
   saddr.sin_addr.s_addr = addr->s_addr;
   saddr.sin_port        = port;

   // OK, we have a src/dst address, and a srcfd, get a fd, then make the epoll
   // descriptor and start working the session.
   errno = 0;

   if ((fd=socket(PF_INET, SOCK_STREAM, 0)) < 0) {
      LG_ERROR(gLog, "NET: Cannot open socket, err=%d", errno);
      res = NET_NO_SOCKET;
   }
   else if ((res=setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (int *) &flag, sizeof(int))) < 0) {
      LG_ERROR(gLog, "NET: Cannot set reuseaddr on socket, err=%d", errno);
      res = NET_NO_REUSEADDR;
   }
   else if (transparent != 0 && (res=setsockopt(fd, SOL_IP, IP_TRANSPARENT, (int *) &flag, sizeof(int))) < 0) {
      LG_ERROR(gLog, "NET: Cannot make socket transparent, wrong kernel version or permissions? Need [2.6.39-IST-WTCP2-b060214]+., err=%d", errno);
      res = NET_NO_TRANSPARENT;
   }
   else if ((res=bind(fd, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
      LG_ERROR(gLog, "NET: Cannot bind socket to address, err=%d", errno);
      res = NET_NO_BIND;
   }  
   else if (noblock != 0 && (res=fcntl(fd, F_SETFL, O_NONBLOCK)) < 0) {
      LG_ERROR(gLog, "NET: Cannot set socket to non-blocking mode, err=%d", errno);
      res = NET_NO_NOBLOCK;
   }  

   if (res < 0 && fd > 0) {
      close(fd);
   }

   return ((res < 0) ? res : fd);
}


int
bind_to_interface(int fd, char *d) {
   int res = 0;
   struct ifreq ifr;

   if (d == NULL || *d == '\0') {
      LG_DEBUG(gLog, "NET: Cannot bind to interface, NULL interface name");
      return -1;
   }
   
   snprintf(ifr.ifr_name, sizeof(ifr.ifr_name)-1, "%s", d);
   ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';

   if ((res=setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (void *) &ifr, sizeof(ifr))) < 0) {
      LG_ERROR(gLog, "NET: Cannot bind socket to [%s], err=%d", d, errno);
      res = NET_NO_SOCKETBIND;
   }

   return res;
}


int
set_congestion(int fd, const char *c) {
   int res = 0;
   int markres = 0;
   int flag = 1;

   if (c == NULL || *c == '\0') {
      LG_DEBUG(gLog, "NET: Cannot set congestion control on socket, NULL algorithm");
      return -1;
   }

   if ((res=setsockopt(fd, SOL_TCP, TCP_CONGESTION, (void *) c, strlen(c))) < 0) {
      LG_ERROR(gLog, "NET: Cannot set congestion control [%s] on socket, err=%d", c, errno);
      res = NET_NO_CONGESTION;
   }

   if ((markres=setsockopt(fd, SOL_SOCKET, SO_MARK, (void *) &flag, sizeof(flag))) < 0) {
      LG_ERROR(gLog, "NET: Cannot set socket mark, err=%d", c, errno);
      markres = NET_NO_MARK;
   }

   return (res < 0) ? res : markres;
}


int
getEPollFD(int epfd, int fd, int events) 
{
   struct epoll_event ev;
   int isnew = FALSE;
   int res = 0;

   if (epfd == 0) {
      if ((epfd=epoll_create(EPOLL_QUEUE_LEN)) < 0) {
         res = NET_NO_EPFD;
      }
      else {
         isnew = TRUE;
      }
   }

   if (epfd > 0) {
      ev.events = EPOLL_BASE|events;
      ev.data.fd = fd;

      if ((res=epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev)) < 0) {
         res = NET_NO_SETADD;
      }
   }

   if (res < 0 && isnew == TRUE) { 
      if (epfd > 0) {
         close(epfd);
      }
   }

   return ((res != 0) ? res : epfd);
}


in_addr_t
siptoaddr(const char *sip) {
   char *s = (char*)sip;
   char *p;
   in_addr_t result = 0;
   
   if (EOS(s) == TRUE) {
      return result;
   }

   if (strncmp(s, "0x", 2) == 0) {
      result = (in_addr_t) strtol(s,NULL,16);
   }
   else {
      int i;

      for (i=3; i>=0 && *s != '\0' && s != p; i--, s=p+1) {
         result += (((uint32_t) strtol(s,&p,10)) << (i*8));
      }

      if (i != -1) { 
         result = 0;
      }
   }

   return result;
}
