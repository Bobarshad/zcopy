#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


/* NOTE: listening socket must have IP_TRANSPARENT set otherwise it will not receive the 
 * inbound connection. The original destination IP addr and port can the be found using
 * a getsockname call as show below.
 *
 * Also can use, this for NATTED sessions:
 *   
 *   struct sockaddr_in sin;
 *   socklen_t sinlen = sizeof(sin);
 *   
 *   if (getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, , ) != 0) {
 *           // handle error 
 *   } else {
 *           // success, address is in sin
 *   }
 *   
 * For UDP sockets, it is a bit more problematic:
 *
 * TProxy's original address sockopt is useful for UDP sockets, where every packet may have 
 * different original destination addresses. In this case, you have to be able to receive all 
 * information atomically, with one system call. So, you have to enable receiving of original 
 * address information with a setsockopt(), and then use recvmsg() to receive the message. 
 * Then, the necessary information should be in the auxiliary information block of the msghdr 
 * structure. But you need this only for UDP, the TCP case is much more simple.  
 */

#define IP_TRANSPARENT 19

int main(int argc, char **argv) {
   int list_s, epfd, conn_s;
   struct sockaddr_in servaddr;
   struct sockaddr_in clntaddr;
   struct sockaddr_in myaddr;
   struct epoll_event ev;
   struct epoll_event events[5];
   socklen_t addrlen;
   int tos = 1;
   int nfds;

   list_s = socket(PF_INET, SOCK_STREAM, 0);
   
   memset(&clntaddr, 0, sizeof(clntaddr));
   clntaddr.sin_family      = AF_INET;
// 172.16.10.20
   clntaddr.sin_addr.s_addr = htonl(0xac100a14);
   clntaddr.sin_port        = htons(13831);
   addrlen = sizeof(clntaddr);
 
   memset(&servaddr, 0, sizeof(servaddr));
   servaddr.sin_family      = AF_INET;
// 192.168.10.20
   servaddr.sin_addr.s_addr = htonl(0xc0a80a15);
//98.189.230.167
//   servaddr.sin_addr.s_addr = htonl(0x62bde6a7);
   servaddr.sin_port        = htons(80);
   addrlen = sizeof(servaddr);
 
   setsockopt(list_s, SOL_SOCKET, SO_REUSEADDR, (int *) &tos, sizeof(int));
   setsockopt(list_s, SOL_IP, IP_TRANSPARENT, (int *) &tos, sizeof(int));
   fcntl(list_s, F_SETFL, O_NONBLOCK);
   bind(list_s, (struct sockaddr *) &clntaddr, sizeof(clntaddr));

   epfd = epoll_create(10);

//   ev.events = EPOLLIN|EPOLLOUT|EPOLLHUP;
//   ev.events = EPOLLOUT|EPOLLIN|EPOLLET|EPOLLHUP;
   ev.events = 0xffff;
   ev.data.fd = list_s;
   epoll_ctl(epfd, EPOLL_CTL_ADD, list_s, &ev);
   
   /*  Enter an infinite loop to respond
       to client requests and echo input  */

   connect(list_s, (struct sockaddr *) &servaddr, addrlen);
   fprintf(stderr, "EPI=0x%04x EPO=0x%04x EPR=0x%04x EPH=0x%04x\n", EPOLLIN,EPOLLOUT,EPOLLERR,EPOLLHUP);

   while ( 1 ) {
      int i;
      nfds = epoll_wait(epfd, events, 5, 10);

      fprintf(stderr, "exit wait, nfds=%d\n", nfds);

      for ( i=0; i< nfds; i++) {
         int flag = events[i].events;
         int fd   = events[i].data.fd;

         fprintf(stderr, "fd=%d, flags=0x%04x\n", fd, flag);
         if (flag == 0x0004) {
            char *msg="GET http://srvr.ist.net/\r\n";
            write(list_s, msg, strlen(msg));
         }
      }

      sleep(2);
   }
}
