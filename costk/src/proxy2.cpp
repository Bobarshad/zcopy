#include "proxy2.h"

Proxy2::Proxy2(int fd_src, int fd_dst, int fd_notify) {
   setIdle();
   fdSrc = fd_src;
   fdDst = fd_dst;
   fdNotify = fd_notify;
}

Proxy2::~Proxy2() {

}

int Proxy2::run()
{
   return 0;
}

void Proxy2::init()
{

}
