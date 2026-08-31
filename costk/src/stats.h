#ifndef __STATS_INC__
#define __STATS_INC__

#include "extern.h"
#include "clist.h"


class Statistics : public Runnable {
private:
   list<Manager *> mlist;
   int port;

protected:
   int run();
   int end();

public:
   Statistics(list<Manager *> mgrs,  int prt);
   virtual ~Statistics();
};
#endif
