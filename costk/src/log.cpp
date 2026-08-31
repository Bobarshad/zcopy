/* SYNOPSIS
 *      A logging class.
 * 
 * DESCRIPTION
 *      Provides a simple thread safe logging facility. Note that sometimes
 *      it may be necessary to increase the logging thread priority to ensure
 *      that debug data is written out.  b
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

#include "log.h"

Log::Log(char *tag, int level, const char *facility, bool showDebug) : Runnable(((tag==NULL)?tag:(char*) facility),0,2) {
   char buf[20];

   mLevel = level;
   mShowDebug = showDebug;
   mFacility = setFacility(facility);
   mLogTag = ((tag != NULL) ? tag : "tlgr");

   sprintf(buf, "%s-lg(%d)", ((tag != NULL) ? tag : facility), mLevel);
   setName(buf);

   // set stdout, stderr to line buffered mode
   setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
   setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

   // if facility is not stderr, then open the logger
   if (mFacility != L_FACILITY_STDERR && mFacility != L_FACILITY_NONE) {
      openlog(mLogTag, LOG_PID, mFacility);
   }

   info("Initialize logging service %s", getName());
   start();
}


Log::~Log() {
   int size;

   info("Terminate logging service %s", getName());
   flush(false);  

   if (mFacility != L_FACILITY_STDERR && mFacility != L_FACILITY_NONE) {
      closelog();
   }
}


int
Log::setFacility(const char *facility) {
   if (facility != NULL) {
      if (strcmp("stderr", facility) == 0) {
	 return L_FACILITY_STDERR;
      }
      else if (strcmp("daemon", facility) == 0) {
	 return L_FACILITY_DAEMON;
      }
      else if (strcmp("none", facility) == 0) {
	 return L_FACILITY_NONE;
      }
      else if (strncmp("local", facility, 5) == 0) {
	 if (strlen(facility) == 6) {
	    int i = '7'-facility[5];

	    if (i >=0 && i <= 7) {
	       return LOG_LOCAL0+i;
	    }
	 }
      }
   }

   return L_FACILITY_DEFAULT;
}


void 
Log::log(int level, bool debug, const char *fmt, va_list args) {
   char buf[2048];

   if (level > mLevel || mFacility == L_FACILITY_NONE) {
      return;
   }

   vsnprintf(buf, 2047, fmt, args);
   LogEntry *item = new LogEntry(level, debug, buf);

   mQueue.push_back(item);
}


void
Log::debug(const char *fmt, ...) {
   va_list ap;
   va_start(ap,fmt);
   log(L_DEBUG,true,fmt,ap);
   va_end(ap);
}

void
Log::info(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log(L_INFO,false,fmt,ap);
  va_end(ap);
}


void
Log::notice(const char *fmt, ...) {
   va_list ap;
   va_start(ap,fmt);
   log(L_NOTICE,false,fmt,ap);
   va_end(ap);
}


void
Log::warn(const char *fmt, ...) {
   va_list ap;
   va_start(ap,fmt);
   log(L_WARN,false,fmt,ap);
   va_end(ap);
}


void
Log::error(const char *fmt, ...) {
   va_list ap;
   va_start(ap,fmt);
   log(L_ERROR,false,fmt,ap);
   va_end(ap);
}


void
Log::critical(const char *fmt, ...) {
   va_list ap;
   va_start(ap,fmt);
   log(L_CRIT,false,fmt,ap);
   va_end(ap);
}


void
Log::alert(const char *fmt, ...) {
   va_list ap;
   va_start(ap,fmt);
   log(L_ALERT,false,fmt,ap);
   va_end(ap);
}


void
Log::fatal(const char *fmt, ...) {
   va_list ap;
   va_start(ap,fmt);
   log(L_FATAL,false,fmt,ap);
   va_end(ap);
}


int 
Log::run() {
   do {
      setThreadName();
      flush(true);
      mQueue.waitFor();
   }
   while (isTerminated() == false);

   return 0;
}


void
Log::flush(bool yield) {
   bool empty;
   int  count=0;

   while (mQueue.isEmpty() == false) {
      LogEntry *item = mQueue.front();

      mQueue.pop_front();

      if (item->isDebug() == false ||  mShowDebug == true) {
	 string type;

	 switch(item->getLevel()) {
	    case L_DEBUG:  type = "DBG"; break;
	    case L_INFO:   type = "INF"; break;
	    case L_NOTICE: type = "NOT"; break;
	    case L_WARN:   type = "WRN"; break;
	    case L_ERROR:  type = "ERR"; break;
	    case L_FATAL:  type = "FTL"; break;

	    default: type = "LOG"; break;
	 }

         if (mFacility == L_FACILITY_STDERR) {
	   fprintf(stderr, "%s: %s: %s\n", mLogTag, type.c_str(), item->getMsg());
         }
         else {
	   syslog(item->getLevel(), "%s: %s\n", type.c_str(), item->getMsg());
         }
      }

      if (yield && (++count > LOG_YIELD_COUNT)) {
	 sched_yield();
         count = 0;
      }

      delete item;
   }
}
