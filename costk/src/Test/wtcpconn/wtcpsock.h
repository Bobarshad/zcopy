#include <string>
#include <arpa/inet.h>

class Socket
{
public:
    Socket();
    ~Socket();
    void make_socket(std::string, int);
    void sockArrayPush(int);
    void sockArrayPull(int);
    void w_connect();
    void w_send(char *);
    void w_bind();
    void w_listenAndProcess();
    void error(std::string);
    int maxConn;

private:
    int sockfd, listsockfd;
    char buf[256];
    struct sockaddr_in serv_addr, cli_addr;
    int sockArray[200000];
};
