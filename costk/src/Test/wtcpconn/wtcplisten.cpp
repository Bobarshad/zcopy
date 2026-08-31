#include "wtcpsock.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

int main(int argc, char **argv)
{
    string addr = "127.0.0.1";
    int port = 1337;
    int connections = 10;

    if(argc > 4)
    {
        fprintf(stderr, "USAGE: %s [address] [port] [number of connections]\n", argv[0]);
        exit(0);
    }

    else if(argc > 3)
    {
        addr = argv[1];
        port = atoi(argv[2]);
        connections = atoi(argv[3]);
    }

    else
        fprintf(stdout, "USING DEFAULT CONNECTION PARAMATERS\n");

    Socket *s = new Socket();
    s->maxConn = connections;
    s->make_socket(addr, port);

    s->w_bind();
    s->w_listenAndProcess();
}
