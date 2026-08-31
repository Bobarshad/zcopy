#ifndef __CSTK_INC__
#define __CSTK_INC__

#define NET_INV_LPORT      -100000
#define NET_NO_EPFD        -100001
#define NET_NO_SETADD      -100002
#define NET_NO_SOCKET      -100003
#define NET_NO_BIND        -100004
#define NET_NO_LISTEN      -100005
#define NET_NO_TRANSPARENT -100006
#define NET_NO_REUSEADDR   -100007
#define NET_NO_NOBLOCK     -100008
#define NET_NO_CONNECT     -100009
#define NET_NO_CONGESTION  -100010
#define NET_NO_SOCKETBIND  -100011
#define NET_NO_MARK        -100012

#ifndef IP_TRANSPARENT
#define IP_TRANSPARENT 19
#endif

#ifndef SOL_TCP
#define SOL_TCP 6
#endif

#ifndef SO_MARK
#define SO_MARK	36
#endif

#define TRUE  1
#define FALSE 0

#define EPOLL_BASE         (EPOLLHUP|EPOLLERR)
#define EPOLL_NONE         0
#define EPOLL_FD_COUNT     2
#define EPOLL_QUEUE_LEN    10
#define EPOLL_TIMEOUT      500

#define EPOLL_ACTIVE_TIMEOUT     300
#define EPOLL_INACTIVE_MULTIPLE  3

extern const char *cca;
#endif
