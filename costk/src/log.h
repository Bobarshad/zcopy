/* SYNOPSIS
 *      A logging class.
 * 
 * DESCRIPTION
 *      Provides a simple thread safe logging facility.
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

#ifndef __LOG_INC__
#define __LOG_INC__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <list>
#include "runnable.h"
#include "logentry.h"
#include "clist.h"

using namespace std;

#define L_DEBUG   LOG_DEBUG	// level = 7
#define L_INFO    LOG_INFO	// level = 6
#define L_NOTICE  LOG_NOTICE	// level = 5
#define L_WARN    LOG_WARNING	// level = 4
#define L_ERROR   LOG_ERR	// level = 3
#define L_CRIT    LOG_CRIT      // level = 2
#define L_ALERT   LOG_ALERT	// level = 1
#define L_FATAL   LOG_EMERG	// level = 0
#define L_DEFAULT L_NOTICE

#define L_FACILITY_LOCAL0 LOG_LOCAL0
#define L_FACILITY_LOCAL1 LOG_LOCAL1
#define L_FACILITY_LOCAL2 LOG_LOCAL2
#define L_FACILITY_LOCAL3 LOG_LOCAL3
#define L_FACILITY_LOCAL4 LOG_LOCAL4
#define L_FACILITY_LOCAL5 LOG_LOCAL5
#define L_FACILITY_LOCAL6 LOG_LOCAL6
#define L_FACILITY_LOCAL7 LOG_LOCAL7
#define L_FACILITY_DAEMON LOG_DAEMON
#define L_FACILITY_STDERR (LOG_NFACILITIES+1000)
#define L_FACILITY_NONE   (LOG_NFACILITIES+1001)
#define L_FACILITY_DEFAULT L_FACILITY_STDERR

#define LOG_YIELD_COUNT  10

#ifdef DEBUG
#   define LG_DEBUG(L,f, ...)   (L)->debug(f, ## __VA_ARGS__);
#else
#   define LG_DEBUG(L,f, ...)
#endif

#define LG_INFO(L,f, ...)    (L)->info(f, ## __VA_ARGS__);
#define LG_NOTICE(L,f, ...)  (L)->notice(f, ## __VA_ARGS__);
#define LG_WARNING(L,f, ...) (L)->warn(f, ## __VA_ARGS__);
#define LG_ERROR(L,f, ...)   (L)->error(f, ## __VA_ARGS__);
#define LG_CRIT(L,f, ...)    (L)->critical(f, ## __VA_ARGS__);
#define LG_ALERT(L,f, ...)   (L)->alert(f, ## __VA_ARGS__);
#define LG_FATAL(L,f, ...)   (L)->fatal(f, ## __VA_ARGS__);

class Log : public Runnable {
private:
   CList<LogEntry *> mQueue;
   const char *mLogTag;
   int mLevel;
   int mFacility;
   bool mShowDebug;

protected:
   virtual int run();
   void log(int level, bool debug, const char *fmt, va_list args);
   int setFacility(const char *facility);
   void flush(bool yield);

public:
   Log(char *tag, int level, const char *facility, bool showDebug);
   virtual ~Log();

   void debug(const char *fmt, ...);
   void info(const char *fmt, ...);
   void warn(const char *fmt, ...);
   void notice(const char *fmt, ...);
   void error(const char *fmt, ...);
   void critical(const char *fmt, ...);
   void alert(const char *fmt, ...);
   void fatal(const char *fmt, ...);
};

extern Log *gLog;
#endif
