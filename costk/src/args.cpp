/* git $Id: args.h,v 1.2 2003/03/07 04:35:23 dvadura Exp $
 *
 * SYNOPSIS
 *      A one line description of the file content
 * 
 * DESCRIPTION
 * 	Multi-line detailed description
 *         
 * AUTHOR
 *      Dennis Vadura, mailto:dennis.vadura@gmail.com
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

#include <iostream>
#include "args.h"
#include "version.h"
#include <stdio.h>
#include <libgen.h>

static char *name;

//=====================================================================================
// CONSTUCTORS AND DESTRUCTORS
//=====================================================================================

Args::Args(int argc, char **argv) {
   _argc = argc;
   _argv = argv;
   _args.clear();
}


Args::~Args() {
}


//=====================================================================================
// PRIVATE METHODS
//=====================================================================================

void
Args::put(char *flag, char *val)
{
   if ( val != NULL ) {
      _args[string(flag)] = val;
   } else {
      cout << "No argument value for --" << flag << endl;
      usage(-1, name);
   }
}


void
Args::usage(int exitcode, char *name)
{
   fprintf(stdout, 
           "%s: [--version] [--fg] [--nopid] [--wdog] [--debug] [--useSplice 0|1] [--spliceChunk <bytes>] [--siblingNotifyLimit <n>] [--maxProxyQueueLength <n>] [--heartBeatIP <string>] [--proxyPoolReapInterval <time>s] [--wdogSleep <time>us] [--activeTimeout <time>s] [--inactiveMultiple <n>] [--logLevel 0-4] [--logFacility {stderr (default)|daemon|local0-7}|none] port [port ...]\n", name);
   exit(exitcode);
}

//=====================================================================================
// PROTECTED METHODS
//=====================================================================================

//=====================================================================================
// PUBLIC METHODS, other than constructor/destructor
//=====================================================================================

const char * 
Args::get(const char *flag, const char *def) 
{
   string key(flag);
   ArgsMap::const_iterator it = _args.find(key);

   if (it == _args.end())
      return def;
   else
      return (const char*) _args[key];
}


const int
Args::get(const char *flag, const int def)
{
   string key(flag);
   ArgsMap::const_iterator it = _args.find(key);
   
   if (it == _args.end())
      return def;
   else
      return atoi(_args[key]);
}


const bool
Args::get(const char *flag, const bool def)
{
   string key(flag);
   ArgsMap::const_iterator it = _args.find(key);
   
   if (it == _args.end())
      return def;
   else
      return (atoi(_args[key]) != 0);
}


const int
Args::parse(void) 
{
   int argc = _argc-1;
   char **argv = _argv+1;
   char *cca = getenv("WTCP_CCA");
   
   name = basename(_argv[0]);
   
   put("progName", _argv[0]);
   put("baseName", name);
   put("cca", (char*) ((cca == NULL) ? "" : cca) );

   while(argc > 0) {
      char *p = *argv++;

      if ( p[0] == '-' ) {
         if ( p[1] == '\0' || p[1] != '-' || p[2] == '\0' ) {
            cout << "Missing option name" << endl;
            usage(-1, name);
         }
         
         p = p+2;

         if (strcmp(p,"version") == 0) {
            cout << Version << endl;
            exit(0);
         }
         else if (strcmp(p,"debug") == 0) {
            put("debug", "1");
         }
         else if (strcmp(p,"fg") == 0) {
            put("fg", "1");
         }
         else if (strcmp(p,"wdog") == 0) {
            put("wdog", "1");
         }
         else if (strcmp(p,"nopid") == 0) {
            put("nopid", "1");
         }
         else {
            argc--;
            char *q = *argv++;

            if (strcmp(p, "logLevel") == 0) {
               put("logLevel",q);
            }
            else if (strcmp(p, "heartBeatIP") == 0) {
               put("heartBeatIP", q);
            }
            else if (strcmp(p, "siblingNotifyLimit") == 0) {
               put("siblingNotifyLimit", q);
            }
            else if (strcmp(p, "maxProxyQueueLength") == 0) {
               put("maxProxyQueueLength", q);
            }
            else if (strcmp(p, "useSplice") == 0) {
               put("useSplice", q);
            }
            else if (strcmp(p, "spliceChunk") == 0) {
               put("spliceChunk", q);
            }
            else if (strcmp(p, "bufferPoolReapInterval") == 0) {
               put("bufferPoolReapInterval", q);
            }
            else if (strcmp(p, "proxyPoolReapInterval") == 0) {
               put("proxyPoolReapInterval", q);
            }
            else if (strcmp(p, "activeTimeout") == 0) {
               put("activeTimeout", q);
            }
            else if (strcmp(p, "inactiveMultiple") == 0) {
               put("inactiveMultiple", q);
            }
            else if (strcmp(p, "wdogSleep") == 0) {
               put("wdogSleep", q);
            }
            else if (strcmp(p, "logFacility") == 0) {
               put("logFacility", q);
            }
            else {
               usage(1, name);
            }
         }
      }
      else {
         int port = atoi(p);
         push_back(port);
      }

      argc--;
   }

   return(0);
}


const ArgList::iterator
Args::begin() {
  return ArgList::begin();
}


const ArgList::iterator
Args::end() {
   return ArgList::end();
}
