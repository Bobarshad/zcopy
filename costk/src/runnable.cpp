/* git $Id: args.h,v 1.2 2003/03/07 04:35:23 dvadura Exp $
 *
 * SYNOPSIS
 *      A one line description of the file content
 * 
 * DESCRIPTION
 * 	Multi-line detailed description
 *         
 * AUTHOR
 *      Dennis Vadura, mail:dennis.vadura@gmail.com
 *
 * WWW
 *      http://www.COCOCOCO.com/
 *
 * COPYRIGHT
 *      Copyright (c) 2011 by COCOCOCO Inc.,  All rights reserved.
 * 
 *      This program is NOT free software; you can redistribute it and/or
 *      modify it under the terms of the Software License Agreement Provided
 *      in the file <distribution-root>/readme/license.txt.
 *
 * LOG
 *      Use git to obtain detailed change logs, blame, etc.
 */

#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "runnable.h"

extern "C" pid_t gettid() {
   return syscall(__NR_gettid);
}

//=====================================================================================
// CONSTUCTORS AND DESTRUCTORS
//=====================================================================================

Runnable::Runnable(int stack_size_multiple)
{
   build(DEFAULT_TNAME, 10, stack_size_multiple);
}


Runnable::Runnable(char *tname, int stack_size_multiple)
{
   build(tname, 10, stack_size_multiple);
}


Runnable::Runnable(int priority, int stack_size_multiple)
{
   build(DEFAULT_TNAME, priority, stack_size_multiple);
}


Runnable::Runnable(char *tname, int priority, int stack_size_multiple) 
{
   build(tname, priority, stack_size_multiple);
}


Runnable::~Runnable() 
{
   // Check if valid and running
   if (mValid == true) {
      cancel();

      // if running, and we are not the 
      // Call join only if the delete being done by the parent process
      if (gettid() == mPPID) {
	 pthread_join(mHThread, NULL);
      }
   }
   else if (mHThread != 0) {
      pthread_join(mHThread, NULL);
      mHThread = 0;
   }
}

//=====================================================================================
// PRIVATE METHODS
//=====================================================================================

void
Runnable::build(char *tname, int priority, int stack_size_multiple)
{
   pthread_attr_t attrSched;
   int res;

   // Create the thread and set default SYSTEM scheduling priority, and
   // stack size.
   pthread_attr_init(&attrSched);
   pthread_attr_setscope(&attrSched, PTHREAD_SCOPE_SYSTEM);
   pthread_attr_setstacksize(&attrSched, stack_size_multiple*1024);

   // Reset the remaining state
   initialize();
   setName(tname);
   mPriority = priority;

   // default init creates the thread
   if ((res=pthread_create(&mHThread, (const pthread_attr_t *) &attrSched, &threadStartup, this)) >= 0) {
      // Mark us as being a valid thread.
      mValid = true;
      mPPID  = gettid();

      // Pause to allow the thread to run and initialize, then set the thread name.
      mInitialized.waitFor();
   }
   else {
      mValid = false;
      throw RunnableException(res);
   }
}


void 
Runnable::setSignal(int action, int sig) 
{
   sigset_t sigset;

   sigemptyset(&sigset);
   sigaddset(&sigset, sig);

   pthread_sigmask(action, &sigset, (sigset_t *) NULL);
}


void 
Runnable::setName(char *name)
{
   int  len=sizeof(mName)-3;
   char buf[sizeof(mName)];

   if (name == NULL) {
      return;
   }

   strncpy(buf, name, len);
   buf[len] = '\0';
   snprintf(mName, len+2, "[%s]", buf);
   mName[sizeof(mName)-1] = '\0';

   mNameValid = false;
}


extern "C" void * 
Runnable::threadStartup(void *arg) 
{
   Runnable *ctxt = (Runnable *) arg;
   sigset_t sigsetToBlock;
   int oldstate;

   sigemptyset(&sigsetToBlock);
   sigaddset(&sigsetToBlock, SIGINT);    // interrupt
   sigaddset(&sigsetToBlock, SIGQUIT);   // quit (ASCII FS)
   sigaddset(&sigsetToBlock, SIGABRT);   // abort signal from abort(3)
   sigaddset(&sigsetToBlock, SIGPIPE);   // write to pipe w/o reader
   sigaddset(&sigsetToBlock, SIGALRM);   // Time signal from alarm(2)
   sigaddset(&sigsetToBlock, SIGTERM);   // soft termination from kill.
   sigaddset(&sigsetToBlock, SIGXCPU);   // exceeded cpu limit
   sigaddset(&sigsetToBlock, SIGXFSZ);   // exceeded file size limit
   sigaddset(&sigsetToBlock, SIGPWR);    // power failure
   sigaddset(&sigsetToBlock, SIGHUP);    // hangup
   sigaddset(&sigsetToBlock, SIGKILL);   // someone is trying to kill us, ouch
   sigaddset(&sigsetToBlock, SIGTERM);   // soft termination from kill.
   pthread_sigmask(SIG_BLOCK, &sigsetToBlock, (sigset_t *) NULL);

   sigemptyset(&sigsetToBlock);
   sigaddset(&sigsetToBlock, SIGCHLD);   // child stopped or terminated
   sigaddset(&sigsetToBlock, SIGSEGV);   // segmentation violation
   sigaddset(&sigsetToBlock, SIGFPE);    // floating point exception
   sigaddset(&sigsetToBlock, SIGBUS);    // bus error
   sigaddset(&sigsetToBlock, SIGFPE);    // floating point exception
   sigaddset(&sigsetToBlock, SIGUSR1);   // user signal 1
   sigaddset(&sigsetToBlock, SIGUSR2);   // user signal 2
   pthread_sigmask(SIG_UNBLOCK, &sigsetToBlock, (sigset_t *) NULL);

   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldstate);
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,&oldstate);

   // Set our priority to requested priority (default is 10 for all threads)
   ctxt->setPriority(ctxt->mPriority);

   ctxt->mTid = pthread_self();
   ctxt->mRunning = false;

   // set the thread name
   if (ctxt->mName[0] == '\0') {
      sprintf(ctxt->mName, "THR:P%d:T%d", getpid(), ctxt->mTid);
   }
   ctxt->setThreadName();

   // The thread indicates that it has successfully initialized itself.
   ctxt->mInitialized.raise();

   // Now we wait to be started by our creator
   ctxt->mStart.waitFor();

   // Yes do it again, incase the name was changed before start() was called
   ctxt->setThreadName();

   // When started, we simply call run, and then end with the result.
   ctxt->mRunning = true;
   ctxt->mResult = ctxt->run();

   // once we return we are no longer valid
   ctxt->mRunning = false;
   ctxt->mValid = false;

   // Raise SIGCHLD in the parent when this happens, so that parent can call pthread_join
   // to reap us.
   kill(ctxt->mPPID, SIGCHLD);

   // Return the thread context after running is complete so that the context
   // can be properly reaped.
   return((void *) ctxt);
}


//=====================================================================================
// PROTECTED METHODS
//=====================================================================================

void 
Runnable::blockSignal(int sig) 
{
   setSignal(SIG_BLOCK, sig);
}

   
void 
Runnable::unblockSignal(int sig) 
{
   setSignal(SIG_UNBLOCK, sig);
}


int 
Runnable::wait(int sig) 
{
   sigset_t sigsetToWait;

   if (mHThread == 0 || mTid != gettid()) {
      return ESRCH;
   }

   sigemptyset(&sigsetToWait);
   sigaddset(&sigsetToWait, sig);

   // sigwait is fine, but make sure that wait is called by the thread that
   // owns this object, note that making this a private method is insufficient
   // as a second instance could call the method on a first instance thereby
   // violating the owning thread requirement.
   return sigwait(&sigsetToWait, &sig);
}


int
Runnable::wait(Condition& cond, int timeout)
{
   return cond.waitFor(timeout);
}


void
Runnable::raise(Condition& cond)
{
   cond.raise();
}


void
Runnable::initialize()
{
   mTid = 0;
   mValid = false;
   mStopped = false;
   mCancelled = false;
   mNameValid = false;
   mName[0] = '\0';
   mSaveName[0] = '\0';
   mResult = 0xDEADBEEF;
}


void
Runnable::pushName(char *name) 
{
   if (name == NULL) {
      return;
   }

   strncpy(mSaveName, mName, sizeof(mSaveName)-1);
   mSaveName[sizeof(mSaveName)-1] = '\0';
   setName(name);

   setThreadName();
}


void
Runnable::popName() 
{
   if (mSaveName[0] == '\0') {
      return;
   }

   strncpy(mName, mSaveName, sizeof(mName)-1);
   mSaveName[sizeof(mSaveName)-1] = '\0';
   mSaveName[0] = '\0';
   mNameValid = false;

   setThreadName();
}


void
Runnable::setThreadName() 
{
   if (mNameValid == false) {
      prctl(PR_SET_NAME, mName, 0,0,0);
   }

   mNameValid = true;
}


//=====================================================================================
// PUBLIC METHODS, other than constructor/destructor
//=====================================================================================

int 
Runnable::signal(int sig) const
{
   return pthread_kill(mHThread, sig);
}
   

char *
Runnable::getName()
{
   return mName;
}


bool
Runnable::isValid() const
{
   return mValid;
}


bool 
Runnable::isStopped() const
{
   return mStopped;
}


bool 
Runnable::isCancelled() const
{
   return mCancelled;
}


void
Runnable::stop() 
{
   mStopped = true;
}


void
Runnable::cancel()
{
   stop();
   mCancelled = true;
   pthread_cancel(mHThread);
   mValid = false;
}


void
Runnable::start()
{
   // Signal the thread to call run(), and yield the run queue
   // so that if it is the same priority as us then the
   // thread will run next.
   mStart.raise();
   sched_yield();
}


void
Runnable::waitStart()
{
   mStart.waitFor();
}


int
Runnable::getPriority() const
{
   return (isValid() ? getpriority(PRIO_PROCESS, this->mTid) : ESRCH);
}


int
Runnable::setPriority(int prio) const
{
   return (isValid() ? setpriority(PRIO_PROCESS, this->mTid, prio) : ESRCH);
}
