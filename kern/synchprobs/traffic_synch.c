#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

 typedef struct Traffic
 {
	 Direction origin;
	 Direction destination;
 }Traffic;
 
 
/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

static struct lock *intersectionLock;
static struct cv *intersectionCV;
struct array *vehicles;

// forward declarations
bool right_turn(Traffic *car);
bool check_condition(Traffic *car1, Traffic *car2);
bool check_all_pairs(Traffic *car); 

// Check if route turns right
bool
right_turn(Traffic *car) {
	KASSERT(car != NULL);
	return (((car->origin == west) && (car->destination == south)) ||
			((car->origin == south) && (car->destination == east)) ||
			((car->origin == east) && (car->destination == north)) ||
			((car->origin == north) && (car->destination == west)));
}

// Check if car1 and car2 can be in the intersection simultaneously
bool
check_condition(Traffic *car1, Traffic *car2) {
	KASSERT(car1 != NULL && car2 != NULL);
	return ((car1->origin == car2->origin) || 
			((car1->origin == car2->destination) &&
			(car1->destination == car2->origin)) ||
			((car1->destination != car2->destination) &&
			(right_turn(car1) || right_turn(car2))));
}

// Loop through the array of cars and compare them with the newly arrived 
// car to see if it can join in the intersection
bool
check_all_pairs(Traffic *car) {
	for(unsigned int i = 0; i < array_num(vehicles); i++){
		bool pass = check_condition(array_get(vehicles, i), car);
		if(!pass){
			cv_wait(intersectionCV, intersectionLock);
			return false;	//violated the rule, wait
		}
	}
	
	KASSERT(lock_do_i_hold(intersectionLock));
	array_add(vehicles, car, NULL);
	return true;	//join the intersection
}



/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
	intersectionCV = cv_create("intersectionCV");
	KASSERT(intersectionCV != NULL);

	intersectionLock = lock_create("intersectionLock");
	KASSERT(intersectionLock != NULL);

	vehicles = array_create();
	array_init(vehicles); 
	KASSERT(vehicles != NULL);
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
	KASSERT(intersectionLock != NULL);
	KASSERT(intersectionCV != NULL);
	KASSERT(vehicles != NULL);

	lock_destroy(intersectionLock);
	cv_destroy(intersectionCV);
	array_destroy(vehicles);
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

void
intersection_before_entry(Direction origin, Direction destination) 
{
	KASSERT(intersectionLock != NULL);
	KASSERT(intersectionCV != NULL);
	KASSERT(vehicles != NULL);

	lock_acquire(intersectionLock);

	Traffic *car = kmalloc(sizeof(struct Traffic));
	KASSERT(car != NULL);
	
	car->origin = origin;
	car->destination = destination;

	while(!check_all_pairs(car)){}

	lock_release(intersectionLock);  
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
	KASSERT(intersectionLock != NULL);
	KASSERT(intersectionCV != NULL);
	KASSERT(vehicles != NULL);

	lock_acquire(intersectionLock);

	for(unsigned int i = 0; i < array_num(vehicles); i++){
		Traffic *car = array_get(vehicles, i);
		if(car->origin == origin && car->destination == destination){
			array_remove(vehicles, i);
			cv_broadcast(intersectionCV, intersectionLock);
			break;
		}
	}
	
	lock_release(intersectionLock);
}

