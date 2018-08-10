#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/*
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/*
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct cv* ncv;
static struct cv* scv;
static struct cv* ecv;
static struct cv* wcv;
static struct lock* trafficlock;
int n_counter = 0;
int e_counter = 0;
int s_counter = 0;
int w_counter = 0;
int i_counter = 0;
Direction d_right_now = north;


/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 *
 */

 bool rt(Direction origin, Direction destination){
   if (origin == north && destination == west){
     return true;
   } else if (origin == east && destination == north){
     return true;
   } else if (origin == south && destination == east){
     return true;
   } else if (origin == west && destination == south){
     return true;
   } else {
     return false;
   }
 }

 int max(int a, int b){
     if(a > b) {
       return a;
     } else {
       return b;
     }
 }

 int min(int a, int b){
     if(a <b) {
       return a;
     } else {
       return b;
     }
 }

void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
  ncv = cv_create("ncv");
  scv = cv_create("scv");
  ecv = cv_create("ecv");
  wcv = cv_create("wcv");
  trafficlock = lock_create("trafficlock");
  return;
}

/*
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  cv_destroy(ncv);
  cv_destroy(scv);
  cv_destroy(ecv);
  cv_destroy(wcv);
  lock_destroy(trafficlock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

volatile bool first = true;

void
intersection_before_entry(Direction origin, Direction destination)
{
  lock_acquire(trafficlock);
  (void)destination;
  if (first){
    first = false;
    d_right_now = origin;
  }

  while (!(d_right_now == origin)) {
    if (origin == north){
      n_counter++;
      cv_wait(ncv, trafficlock);
      n_counter--;
    } else if (origin == south){
      s_counter++;
      cv_wait(scv, trafficlock);
      s_counter--;
    } else if (origin == east){
      e_counter++;
      cv_wait(ecv, trafficlock);
      e_counter--;
    } else if (origin == west){
      w_counter++;
      cv_wait(wcv, trafficlock);
      w_counter--;
    }
  }
  i_counter++;
  lock_release(trafficlock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination)
{
  lock_acquire(trafficlock);
  i_counter--;
  int biggest_queue = max(max(n_counter,s_counter), max(e_counter,w_counter));
    if (biggest_queue == 0 && i_counter==0){
      first = true;
    }
    if (i_counter == 0){
      if (biggest_queue == n_counter){
        d_right_now = north;
        cv_broadcast(ncv,trafficlock);
      }
      if(biggest_queue == s_counter){
        d_right_now = south;
        cv_broadcast(scv,trafficlock);
      }
      if(biggest_queue == e_counter){
        d_right_now = east;
        cv_broadcast(ecv,trafficlock);
      }
      if(biggest_queue == w_counter){
        d_right_now = west;
        cv_broadcast(wcv,trafficlock);
      }
    }
  (void)origin;
  (void)destination;
  lock_release(trafficlock);
}
