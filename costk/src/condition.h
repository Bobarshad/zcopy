/* SYNOPSIS
 *      A simple condition variable based on a semaphore
 * 
 * DESCRIPTION
 * 	Uses pthread_wait and a semaphore to create a condition variable that can be 
 *      easily included, waited on, fired or derrived from.
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

#ifndef __CONDITION_INC__
#define __CONDITION_INC__

#include <sys/types.h>
#include "semaphore.h"

#define NSEC_IN_ONESEC 1000000000;

class Condition : Semaphore {
private:
   pthread_cond_t  mp_cond;
   int             fired;

public:
   Condition() {
      fired = 0;
      pthread_cond_init(&mp_cond, NULL);
   }

   ~Condition() {
      pthread_cond_broadcast(&mp_cond);
   }

   inline int waitFor(u_int64_t nsec_timeout=0) {
      int res=0;

      P();
      if (fired > 0) {
         fired--;
         V();
         return(res);
      }

      if (nsec_timeout == 0) {
         pthread_cond_wait(&mp_cond, getSEM());
         fired--;
      }
      else {
         struct   timespec ts;
         u_int64_t nsec;
         u_int64_t sec;

         clock_gettime(CLOCK_REALTIME, &ts);
         sec = nsec_timeout/NSEC_IN_ONESEC;       
         nsec = nsec_timeout-sec*NSEC_IN_ONESEC;

         /* FIXME: Compensate for the time it takes to compute the nsec_timeout value, 
          *        is 5ns the right #? */
         ts.tv_nsec += nsec+5;
         ts.tv_sec  += sec;

         if((res=pthread_cond_timedwait(&mp_cond, getSEM(), &ts)) >= 0) {
            fired--;
         }
      }
      V();

      return res;
   }


   inline void raise() {
      P();
      fired++;
      pthread_cond_signal(&mp_cond);
      V();
   }


   inline void reset() {
      P();
      pthread_cond_broadcast(&mp_cond);
      fired=0;
      V();
   }
};
#endif
