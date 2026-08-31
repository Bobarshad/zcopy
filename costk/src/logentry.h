/* SYNOPSIS
 *      A simple struct to represent an entry in the log
 * 
 * DESCRIPTION
 * 	Encapsulates log entries before they are written out.  
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

#ifndef __LOGENTRY_INC__
#define __LOGENTRY_INC__

#include <string>

using namespace std;

struct LogEntry {
private:
   string mMsg;
   int    mLevel;
   bool   mDebug;

public:
   LogEntry(int level, bool debug, char *msg);
   virtual ~LogEntry();

   int getLevel();
   int isDebug();
   const char *getMsg();
};

#endif
