////////////////////// cstk.cpp /////////////////////// 
// 
// (c) Copyright 2011 IST International, based on demo file from runnable package.
//
// Project: IST WTCP GW
//
// Author(s): DVadura
//
// PURPOSE:
// main line for the IST WTCP GW
//
//////////////////////////////////////////////////////// 

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>


//
/////////////////////// INCLUDE FILES ///////////////////////
#define  DEFINE_EXTERNALS 1

#include "extern.h"

//
///////////////////////// Default main constant defs ////////////////


//
///////////////////////// Global Data defs ///////////////////////// 
Log  *gLog;
Args *gArgs;
const char *cca;


//
///////////////////////// Local Static defs ///////////////////////// 
static int  sigpipe[2];
static bool nopid = false;
static bool sigTerm = false;
static char *bname;


//
//////////////////// Function defs /////////////////////
void 
CatchUser1(int sig) 
{
}


void 
CatchTerm(int sig) 
{
   sigTerm = true;

   if (sigpipe[1] != -1) {
      write(sigpipe[1], "", 1);
   }
}


void 
CatchSignals(int sig) 
{
}


void 
RestoreChild(int sig) 
{
}


void
write_pid(char *name) {
   char file[64];
   char path[256];
   bool pidOk = false;
   
   // if we are running in watchdog mode then the watchdog tells the kids to not
   // update their pids.
   if (nopid == true) {
      return;
   }

   strncpy(file, name, 63);
   file[63] = '\0';
   sprintf(path, "/var/run/%s.pid", file);

   errno = 0;
   FILE *pid = fopen(path, "w+");

   if (pid != NULL && errno == 0) {
      int res;

      if ((res=fprintf(pid, "%d", getpid())) > 0) {
         LG_NOTICE(gLog, "Wrote PID (%d) to [%s] len=%d", getpid(), path, res);
         pidOk = true;
      }

      fclose(pid);
   }

   if (pidOk == false) {
      LG_WARNING(gLog, "Cannot write PID(%d) to [%s]", getpid(), path);
   }
}


void 
daemon(bool foreground, bool debug) 
{
   ProxyPool *proxyPool;
   Statistics *stats;
   list<Manager *> mlist;

   // setup signals
   signal(SIGINT,  CatchSignals);
   signal(SIGQUIT, CatchSignals);
   signal(SIGTERM, CatchTerm);
   signal(SIGHUP,  CatchSignals);
   signal(SIGUSR1, CatchUser1);

   prctl(PR_SET_NAME, bname, 0,0,0);
   proxyPool = ((debug == true) ? new ProxyPool(2,5) : new ProxyPool(100,500));

   // open server socket
   ArgList::iterator it;
   for (it=gArgs->begin(); it != gArgs->end(); it++) {
      int port = *it;

      LG_DEBUG(gLog, "Start service for port (%d)", port);
      mlist.push_back(new Manager(port, proxyPool));
   }

   LG_NOTICE(gLog, "Main thread pausing...");

   // pause and wait til someone signals us to stop.
   stats = new Statistics(mlist, 100);
   pause();
   delete stats;

   // Ignore further signals
   signal(SIGINT,  SIG_IGN);
   signal(SIGQUIT, SIG_IGN);
   signal(SIGTERM, SIG_IGN);
   signal(SIGHUP,  SIG_IGN);
   signal(SIGUSR1, SIG_IGN);

   // close server socket
   while (mlist.empty() == FALSE) {
      Manager *pmg = mlist.front();
      mlist.pop_front();
      delete pmg;
   }

   delete proxyPool;
}


int 
main(int pArgc, char *pArgv[])
{
   // Parse server arguments
   gArgs = new Args(pArgc, pArgv);
   gArgs->parse();

   // Figure out if we are to run in the foreground
   const char*  logfacility = gArgs->get("logFacility", "stderr");
   cca                      = gArgs->get("cca", "wtcp");
   bool         foreground  = gArgs->get("fg", false);
   int          loglevel    = gArgs->get("logLevel", L_INFO);//L_NOTICE);
   bool         wdog        = gArgs->get("wdog", false);
   bool         debug       = gArgs->get("debug", false);
   int          wdogsleep   = gArgs->get("wdogSleep", 250000);

   sigpipe[0] = -1;
   sigpipe[1] = -1;
   bname      = (char*) gArgs->get("baseName", pArgv[0]);
   nopid      = gArgs->get("nopid", false);

   if (cca == NULL || *cca == '\0') {
      cca = "wtcp";
   }

   // If fg=T, and wdog=T, then child has fg=T wdog=F
   //    fg=T, and wdog=F, then no wdog.
   //    fg=F, and wdog=T, then we disassociate, start child with fg=T, wdog=F
   //    fg=F, and wdog=F, then we disassociate, no wdog.
   // 
   // If foreground is false, then we disassociate from the controling terminal by
   // forking and running in the background.
   //
   if (foreground == false) {
      int child;
      if ((child=fork()) != 0) exit(0);

      close(0);
      close(1);
      close(2);

      // make sure logfacility setting makes sense after we close stdout,stderr
      if (strcmp(logfacility,"stderr") == 0) {
         logfacility = "none";
      }
   }

   gLog = new Log(bname, loglevel, logfacility, debug);

   if (wdog == false) {
      LG_NOTICE(gLog, 
                "Running in %s, without watchdog", (foreground ? "foreground" : "background")); 

      write_pid(bname);
      daemon(true, debug);
   }
   else {
      bool childTerminated = false;

      LG_NOTICE(gLog, 
                "Running in %s, becoming watchdog", (foreground ? "foreground" : "background")); 

      if (pipe(sigpipe) == -1) {
         LG_FATAL(gLog, "[%s] Cannot create pipe", bname);
         exit(-1);
      }

      fcntl(sigpipe[0], F_SETFL, fcntl(sigpipe[0],F_GETFL)|O_NONBLOCK);
      fcntl(sigpipe[1], F_SETFL, fcntl(sigpipe[1],F_GETFL)|O_NONBLOCK);

      signal(SIGINT,  SIG_IGN);
      signal(SIGQUIT, SIG_IGN);
      signal(SIGTERM, CatchTerm);
      signal(SIGHUP,  SIG_IGN);

      // reset our thread name
      prctl(PR_SET_NAME, "wdog", 0,0,0);

      // rename the logger
      gLog->setName("wdog-lg");
      LG_NOTICE(gLog, "Watchdog for [%s] started, sleep=%dus", bname, wdogsleep);

      // write our pid as the watchdog
      write_pid(bname);

      // loop until signaled to stop
      while (sigTerm == false && childTerminated == false) {
         int child;

         if ((child=fork()) > 0) {
            bool childOk = true;

	    // parent, child is the pid of the child, we watch and wait
	    while (childOk == true) {
               fd_set rdset;
               struct timeval tv;
	       int status;
	       int res;

               // waitpid does not get interrupted except by SIGCHLD, and we need a way to stop
               // the watchdog if the users tells us to, so use select, which has a timeout that can
               // be used to check the status of our kids.
	       tv.tv_usec = wdogsleep;
	       tv.tv_sec  = 0;
	       FD_ZERO(&rdset);
	       FD_SET(sigpipe[0], &rdset);

               if ((res=select(sigpipe[0]+1, &rdset, NULL, NULL, &tv)) > 0) {
                  char c[10];
                  while(read(sigpipe[0], c, 10) > 0);
               }
               else {
               }

               res = waitpid(child, &status, WNOHANG|WUNTRACED|WCONTINUED);

	       if (res == -1) {
                  if (errno == EINVAL) {
                     LG_ERROR(gLog, "waitpid: error in options");
                  }

                  // Otherwise we either got a signal, or we have no children to wait for.
                  // either way, exit
	  	  LG_NOTICE(gLog, "GOT TERM SIG %d", child);
	       }
               else if (res != 0) {
		  if (WIFEXITED(status)) {
		     LG_NOTICE(gLog, "Child (%d), normal exit(%d), terminating...", child, WEXITSTATUS(status));
		     childOk = false;
                     childTerminated = true;
		  }
		  else if (WIFSIGNALED(status)) {
		     LG_ERROR(gLog, 
			       "Child (%d), received abnormal signal %d, restarting...", 
			       child, WTERMSIG(status));
		     childOk = false;

                     if (WTERMSIG(status) == 11) {
                        LG_ERROR(gLog, "Proxy Terminating, child received signal 11, fix it...");
                     }
		  }
		  else if (WIFSTOPPED(status)) {
		     LG_NOTICE(gLog, 
			       "Child (%d), received SIGSTOP signal %d, should stop shortly.", 
			       child, WSTOPSIG(status));
		  }
		  else if (WIFCONTINUED(status)) {
		     LG_NOTICE(gLog, 
			       "Child (%d), received SIGCONT, prior stop cancelled.", 
			       child);
		  }
               }

	       // We have terminated, signal our children to do the same.
	       if (sigTerm == true && childOk == true && childTerminated == false) {
		  LG_NOTICE(gLog, "Sending SIGTERM to child %d", child);
		  kill(child, SIGTERM);
                  childOk = false;
	       }
	    }
	 }
	 else {
	    char* nargv[pArgc+10];
            int  j=0;

	    // safety measure to guard against run-away's :-)
            sleep(2);

	    nargv[j++] = pArgv[0];
	    nargv[j++] = "--fg";
            nargv[j++] = "--nopid";
            nargv[j++] = "--logFacility";
            nargv[j++] = (char *) logfacility;

            for (int i=1; i < pArgc; i++) {
               if (strcmp(pArgv[i],"--fg") == 0) {
                  continue;
               }
               else if (strcmp(pArgv[i],"--wdog") == 0) {
                  continue;
               }
               else if (strcmp(pArgv[i],"--logFacility") == 0) {
                  i++;
               }
               else {
                  nargv[j++] = pArgv[i];
               }
            }

	    nargv[j++] = NULL;
	    execv(nargv[0], nargv);
	 }
      }
   }

   LG_NOTICE(gLog, "Normal exit in 2s!");

   // give everyone a chance to flush
   sleep(2);

   // restore further signals, redundant really
   signal(SIGINT,  SIG_DFL);
   signal(SIGQUIT, SIG_DFL);
   signal(SIGTERM, SIG_DFL);
   signal(SIGHUP,  SIG_DFL);

   // We delete the log last so that any pending log messages are properly
   // serviced and flushed if need be.
   delete gLog;

   exit(0);
}
