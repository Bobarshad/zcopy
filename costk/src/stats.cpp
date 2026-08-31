#include "extern.h"

Statistics::Statistics (list<Manager *> mgrs, int prt) {
   char tname[20];

   mlist = mgrs;
   port  = prt;

   LG_DEBUG(gLog, "ST: Create statistics agent on port %d", port);
   sprintf(tname, "ST(%d)", port);
   setName(tname);
   start();
}


Statistics::~Statistics() {
}


int
Statistics::run() {
   struct in_addr addr;
   int flag = 1;
   int lsock = 0;
   int fail = 0;
   int res;
   
   addr.s_addr = htonl(INADDR_ANY);
   if ((res=openSocket(&addr, htons(port), FALSE, FALSE)) > 0) {
      lsock = res; 
   
      if ((res=listen(lsock, 128)) < 0) {
         close(lsock);
         lsock = 0;
      }
   }

   do {
      int fd;
      char buf[200];
      socklen_t addrlen = sizeof(addr);

      if ((fd=accept(lsock, (struct sockaddr *) &addr, &addrlen)) < 0) {
         if (fail == 0) {
            fail = 1;
            continue;
         }

         LG_ERROR(gLog, "STATS: accept failure on port %d, exiting", port);
         break;
      }  

      for (list<Manager *>::iterator mi = mlist.begin(); mi != mlist.end(); mi++) {
         char tot[180];
	 (*mi)->getTotals(tot, 180);
	 sprintf(buf, "%s\n", tot);
	 write(fd, buf, strlen(buf));
      }

      close(fd);
   }
   while (isTerminated() == FALSE);

   LG_DEBUG(gLog, "ST: Close statistics agent on port %d", port);
   close(lsock);
   return 0;
}
