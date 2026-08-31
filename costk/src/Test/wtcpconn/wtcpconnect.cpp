#include <string>
#include <cstdio>
#include <cstdlib>
#include "wtcpsock.h"

int main(int argc, char **argv)
{
    std::string addr = "127.0.0.1";
    int port = 1337;
    int connections = 10; //number of total connections to test for

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

    for(int i = 0; i < connections; ++i)
    {
	char msg[20];
        s->make_socket(addr, port);
        s->w_connect();
        s->sockArrayPush(i);
        fprintf(stdout, "Connections: %d\n", i);
        usleep(1000);
	sprintf(msg, "test %6d", i);
        s->w_send(msg);
    }

    for(int i = 0; i < connections; ++i)
    {
	char msg[20];
        usleep(1000);
        fprintf(stdout, "Resend: %d\n", i);
	sprintf(msg, "resend %6d", i);
        s->sockArrayPull(i);
        s->w_send(msg);
    }
        

    return 0;
}
