/* SYNOPSIS
 *      A simple P,V abstraction of a semaphore
 * 
 * DESCRIPTION
 * 	User pthread_mutex to create a semaphore abstraction so that P(), and V() can
 *      be used to obtain, and release the semaphore.  Uses the default pthread_mutex
 *      implementation.
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

#ifndef __SEMAPHORE_INC__
#define __SEMAPHORE_INC__

#include <pthread.h>

class Semaphore {
private:
   pthread_mutex_t mp_mutex;
   int locked;

protected:
   pthread_mutex_t *getSEM() {
      return (&mp_mutex);
   }

public:
   Semaphore() {
      pthread_mutex_init(&mp_mutex, NULL);
      locked = 0;
   }

   ~Semaphore() {
      if (locked) {
         pthread_mutex_unlock(&mp_mutex);
      }
         
      pthread_mutex_destroy(&mp_mutex);
   }

   inline void P() {
      pthread_mutex_lock(&mp_mutex);
      locked=1;
   }
   
   inline void V() {
      locked=0;
      pthread_mutex_unlock(&mp_mutex);
   }
};
#endif
