#include "wtcpsock.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

Socket::Socket()
{
/*
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        error("socket");

    //default address and port values
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(1337);
*/
}

void Socket::make_socket(string addr, int port)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        error("socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, addr.c_str(), &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);
}

Socket::~Socket()
{
    close(sockfd);
}

void Socket::sockArrayPush(int i)
{
    sockArray[i] = sockfd;
}


// This makes the ith file descriptor stored in sockArray
//   the current file descriptor, sockfd
void Socket::sockArrayPull(int i)
{
    sockfd = sockArray[i];
}

void Socket::w_connect()
{
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("connection");
}

void Socket::w_send(char *str)
{
     int n;
     sprintf(buf, "%s", str);
     n = write(sockfd, buf, strlen(buf));
     if(n < 0)
         error("write");
}

void Socket::w_bind()
{
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("bind");
}

void Socket::w_listenAndProcess()
{
    int n;
    socklen_t clilen;

    listsockfd = sockfd;
    listen(listsockfd, 1024);

    bzero((char *)&cli_addr, sizeof(cli_addr));
    clilen = sizeof(cli_addr);

    for(int i = 0; i < maxConn; ++i)
    {
        sockfd = accept(listsockfd, (struct sockaddr *) &cli_addr, &clilen);
	sockArrayPush(i);

        if(sockfd < 0)
            error("listening");

        n = read(sockfd, buf, 255);
	buf[n] = 0;
        if(n < 0)
            error("read");
        fprintf(stdout, "RECV: %s, count: %d\n", buf, i);
    }

    for(int i = 0; i < maxConn; ++i)
    {
	sockArrayPull(i);
        n = read(sockfd, buf, 255);
	buf[n] = 0;
        if(n < 0)
            error("read");
        fprintf(stdout, "RECV2: %s, count: %d\n", buf, i);
        close(sockfd);
    }

    close(listsockfd);
}

void Socket::error(string msg)
{
    string errmsg = "ERROR: " + msg + "\n";
    //fprintf(stderr, errmsg.c_str()); 
    perror(msg.c_str());
    exit(0);
}
