#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
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
   int list_s, conn_s;
   struct sockaddr_in servaddr;
   struct sockaddr_in clntaddr;
   struct sockaddr_in myaddr;
   socklen_t addrlen;
   int tos = 1;

   list_s = socket(PF_INET, SOCK_STREAM, 0);
   
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(3129);

    /* set it up for transparent proxy support */
    if (setsockopt(list_s, SOL_IP, IP_TRANSPARENT, (char *) &tos, sizeof(int)) <0) {
	fprintf(stderr, "ECHOSERV: Error calling setsockopt() %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    /*  Bind our socket addresss to the 
	listening socket, and call listen()  */
    if ( bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling bind()\n");
	exit(EXIT_FAILURE);
    }

    /* indicate we want to listen on the socket */
    if ( listen(list_s, 1024) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling listen()\n");
	exit(EXIT_FAILURE);
    }
    
    /*  Enter an infinite loop to respond
        to client requests and echo input  */

    while ( 1 ) {
	/*  Wait for a connection, then accept() it  */
        memset(&clntaddr, 0, sizeof(clntaddr));

        addrlen = sizeof(clntaddr);
        errno = 0;
	if ( (conn_s = accept(list_s, (struct sockaddr *) &clntaddr, &addrlen) ) < 0 ) {
	    fprintf(stderr, "ECHOSERV: Error calling accept() %d %d %s\n", conn_s, errno, strerror(errno));
	    exit(EXIT_FAILURE);
	}

        addrlen = sizeof(myaddr);
        memset(&myaddr, 0, sizeof(myaddr));
        getsockname(conn_s, (struct sockaddr *) &myaddr, &addrlen);

	/*  Close the connected socket  */
	if ( close(conn_s) < 0 ) {
	    fprintf(stderr, "ECHOSERV: Error calling close()\n");
	    exit(EXIT_FAILURE);
	}

        /* and voila the ip's are exactly what the client sent. */

        fprintf(stderr,"Connection: %d.%d.%d.%d:%d <= %d.%d.%d.%d:%d \n",
           (myaddr.sin_addr.s_addr >> 0) & 0xff,
           (myaddr.sin_addr.s_addr >> 8) & 0xff,
           (myaddr.sin_addr.s_addr >> 16) & 0xff,
           (myaddr.sin_addr.s_addr >> 24) & 0xff,
           ntohs(myaddr.sin_port),
           (clntaddr.sin_addr.s_addr >> 0) & 0xff,
           (clntaddr.sin_addr.s_addr >> 8) & 0xff,
           (clntaddr.sin_addr.s_addr >> 16) & 0xff,
           (clntaddr.sin_addr.s_addr >> 24) & 0xff,
           ntohs(clntaddr.sin_port));
    }
}
