/* SYNOPSIS
 *      A simple version of the Runnable class for C++
 * 
 * DESCRIPTION
 *      The idea is to make a simple Runnable, that mimics to some degree the
 *      functionality of its Java sister.  Although, I have tried to stick with
 *      the posix pthreads implementation, and used portable C++ as much as 
 *      possible, it is still probably linux only at this time.  Which is all that
 *      I need it for for now.
 *         
 * AUTHOR
 *      Dennis Vadura, mailto:dennis.vadura@gmail.com
 *
 * WWW
 *      http://www.vadura.eu/runnable
 *
 * COPYRIGHT
 *      Copyright (c) 2011 by Dennis Vadura,  All rights reserved.
 * 
 *      You can obtain and redistribute or modify this program under the 
 *      terms of the Software License Agreement Provided in the file 
 *      <distribution-root>/readme/license.txt.
 */

#ifndef __RUNNABLE_INC__
#define __RUNNABLE_INC__

#include <exception>

#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sched.h>
#include "condition.h"

using namespace std;

#define PR_SET_NAME_LEN 16
#define DEFAULT_TNAME   "thread"

extern "C" {
   typedef void * thread_start_fn_t(void *);
}

class RunnableException : public exception {
   char buf[120];
   int errcode;

public:
   RunnableException(int value=-1) {
      errcode = value;
   }

   virtual char* reason() throw() {
      sprintf (buf, "Runnable: Unable to create thread, err=%d", errcode);
      return buf;
   }
};


class Runnable : public Semaphore {
private:
   // The handle for the thread represented by this Runnable object
   pthread_t mHThread;
 
   // The thread's system ID
   pid_t mTid;

   // The thread's parent ID
   pid_t mPPID;

   // The thread's priority
   int mPriority;

   // Indicates if the thread is valid
   bool mValid;

   // Indicates if the thread is running
   bool mRunning;

   // Indicates if the thread is stopped, when running and stopped becomes true
   // the thread will attempt to terminate gracefully.
   bool mStopped;

   // If true the thread was cancelled and is terminated
   bool mCancelled;

   // If false setThreadName() needs to be called
   bool mNameValid;
 
   // The thread name
   char mName[PR_SET_NAME_LEN];

   // The thread name, saved (pushed by pushName);
   char mSaveName[PR_SET_NAME_LEN];

   // Used to indicate that the thread is initialized, waited on by our creator
   Condition mInitialized;

   // Used to synchronize thread startup.
   Condition mStart;

   // Result returned from run.
   int mResult;

   // Helper function to set signals for the thread.
   void setSignal(int action, int sig);

   // Return if this thread has been cancelled
   bool isThreadCancelled();

   // Private builder method
   void build(char *tname, int priority, int stack_size_multiple);

   // Thread Startup initializer function passed to pthread_create
   //static void *threadStartup(void *arg);
   static thread_start_fn_t threadStartup;

protected:
   Runnable(int stack_size_multiple=2);
   virtual int run()=0;

   inline pthread_t getThread() const {return(mHThread);}
   virtual void blockSignal(int sig);
   virtual void unblockSignal(int sig);
   virtual int  wait(int sig);
   virtual int  wait(Condition& cond, int timeout=0);
   virtual void waitStart();
   virtual void raise(Condition& cond);
   virtual void initialize();
   virtual void pushName(char *name);
   virtual void popName();
   virtual void setThreadName();

public:
   Runnable(char *tname, int stack_size_multiple=2);
   Runnable(int priority, int stack_size_multiple=2);
   Runnable(char *tname, int priority, int stack_size_multiple=2);
   virtual ~Runnable();
   
   virtual void start();
   virtual void stop();
   virtual void cancel();
   char *getName();

   virtual int  setPriority(int prio) const;
   virtual void setName(char *name);
   int  signal(int sig) const;
   int  getPriority() const;
   pid_t  getTID() {return mTid;}

   bool isValid() const ;
   bool isStopped() const ;
   bool isCancelled() const ;
   bool isTerminated() {return isValid() && isStopped();}
};
#endif
