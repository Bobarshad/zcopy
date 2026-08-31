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

#ifndef __ARGS_INC__
#define __ARGS_INC__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <utility>
#include <map>
#include <list>

using namespace std;

typedef map<string,char*> ArgsMap;
typedef list<int>         ArgList;

class Args : public ArgList {
private:
   ArgsMap      _args;
   int          _argc;
   char       **_argv;

   void put(char *flag, char *value);
   void usage(int exit_code, char *name);
   
public:
   Args(int argc, char **argv);
   virtual ~Args();
   
   const int   parse(void);
   const char *get(const char *flag, const char *def);
   const bool  get(const char *flag, const bool def);
   const int   get(const char *flag, const int def);

   const ArgList::iterator begin();
   const ArgList::iterator end();
};

extern Args *gArgs;
#endif
