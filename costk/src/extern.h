#ifndef __EXTERN_INC__
#define __EXTERN_INC__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <linux/socket.h>
#include <linux/tcp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sched.h>

// #include <malloc.h>
#include <signal.h>
#include <grp.h>
#include <memory.h>
#include <math.h>         // defines HUGE_VAL, fabs()
#include <errno.h>        // defines errno

#include "runnable.h"
#include "cstk.h"
#include "args.h"
#include "buffer.h"
#include "bfpool.h"
#include "proxy.h"
#include "proxypool.h"
#include "logentry.h"
#include "log.h"
#include "manager.h"
#include "clist.h"
#include "stats.h"

extern "C" int getEPollFD(int epfd, int fd, int events);
extern "C" int openSocket(struct in_addr *saddr, in_port_t sport, int transparent, int noblock);
extern "C" in_addr_t siptoaddr(const char *s);
extern "C" int set_congestion(int fd, const char *c);
extern "C" int bind_to_interface(int fd, char *c);

#endif
