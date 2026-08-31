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

#include "logentry.h"

LogEntry::LogEntry(int level, bool debug, char *msg) {
   mLevel = level;
   mDebug = debug;
   mMsg += msg;
}

LogEntry::~LogEntry() {
}

int
LogEntry::getLevel() {
  return mLevel;
}

int
LogEntry::isDebug() {
   return (mDebug != false);
}

const char *
LogEntry::getMsg() {
   return mMsg.c_str();
}
