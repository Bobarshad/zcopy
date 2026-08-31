OBJS = netutil.o \
       logentry.o \
       stats.o \
       log.o \
       cstk.o \
       args.o \
       runnable.o \
       manager.o \
       proxypool.o \
       proxy.o \
       buffer.o \
       bfpool.o

.PHONY: all
all: cstk

LD_LIBS += -lpthread -lrt
cstk: $(OBJS)
	$(CXX) $(CXXFLAGS) -Wall -O3 -o $@ $^ $(LD_LIBS)

.PHONY: clean
clean:
	rm -fv costk *.o cstk


$(OBJS): extern.h

runnable.o : condition.h semaphore.h runnable.h
args.o     : args.h version.h
stats.o    : stats.h version.h 
manager.o  : manager.h proxy.h proxy.h proxypool.h
proxypool.o: proxy.h proxypool.h
proxy.o    : proxy.h proxypool.h
cstk.o     : cstk.h proxy.h proxypool.h
bfpool.o   : bfpool.h buffer.h
buffer.o   : bfpool.h buffer.h
logentry.o : logentry.h
log.o      : log.h

