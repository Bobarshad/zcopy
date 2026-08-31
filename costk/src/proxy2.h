#ifndef __PROXY2_INC__
#define __PROXY2_INC__

#include "extern.h"

#define PROXY_STATE_ACTIVE    0
#define PROXY_STATE_INACTIVE  1
#define PROXY_STATE_IDLE      2
#define PROXY_STATE_CLOSING   3
#define PROXY_STATE_CLOSED    4

#define PROXY_DATA_INPUTHOLD      0x01
#define PROXY_DATA_OUTPUTHOLD     0x02
#define PROXY_DATA_SIBLINGCLOSED  0x04

class Proxy2 : public Runnable {
private:
   char         state;
   uint8_t      flags;
   uint32_t     fdSrc, fdDst, fdNotify, fdEpoll;
 
protected:
   struct in_addr address;
   in_port_t      port;

   virtual int  run();
   virtual void init();

public:
   Proxy2(int fd_src, int fd_dst, int fd_notify);
   virtual ~Proxy2();
   
   int          isActive()     {return state == PROXY_STATE_ACTIVE;}
   int          isInActive()   {return state == PROXY_STATE_INACTIVE;}
   int          isIdle()       {return state == PROXY_STATE_IDLE;}
   int          isClosed()     {return state == PROXY_STATE_CLOSED;}
   int          isClosing()    {return state == PROXY_STATE_CLOSING;}

   void         setActive()    {state = PROXY_STATE_ACTIVE;}
   void         setInActive()  {state = PROXY_STATE_INACTIVE;}
   void         setIdle()      {state = PROXY_STATE_IDLE;}
   void         setClosed()    {state = PROXY_STATE_CLOSED;}
   void         setClosing()   {state = PROXY_STATE_CLOSING;}

   int          isInputHold()    {return flags & PROXY_DATA_INPUTHOLD;}
   int          setInputHold()   {flags |= PROXY_DATA_INPUTHOLD;} 
   int          clearInputHold() {flags &= ~PROXY_DATA_INPUTHOLD;} 

   int          isOutputHold()    {return flags & PROXY_DATA_OUTPUTHOLD;}
   int          setOutputHold()   {flags |= PROXY_DATA_OUTPUTHOLD;} 
   int          clearOutputHold() {flags &= ~PROXY_DATA_OUTPUTHOLD;} 

   int          isSiblingClosed()    {return flags & PROXY_DATA_SIBLINGCLOSED;}
   int          setSiblingClosed()   {flags |= PROXY_DATA_SIBLINGCLOSED;} 
   int          clearSiblingClosed() {flags &= ~PROXY_DATA_SIBLINGCLOSED;} 

   void         markActive();
   void         markInActive();
   void         markIdle();
};

#endif
