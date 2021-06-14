
/****************************************************************************/
/* Function: nanosleep and POSIX 1003.1b RT clock demonstration             */
/*                                                                          */
/* Sam Siewert - 02/05/2011                                                 */
/* Edited by Michelle Christian						    */
/****************************************************************************/

/************************************************************************Includes*******************************************************************************/

#include <pthread.h>                                                                    	//this header file defines all the posix thread API. 
												//Allows to create multiple threads for concurrent process flow.
#include <unistd.h>                                                                     	//this header file provides access to the POSIX os API. 
#include <stdio.h>                                                                      	//standard input output library
#include <stdlib.h>                                                     
#include <time.h>                                                                       	//contains definitions for functions to get nd manipulate time and date functions
#include <errno.h>                                                                     	        //this header file defines the integer variable errno, which is set by system calls 
                                                                                        	//and some library functions in the vent of an error to indicate what went wrong

/*************************************************************************Macros*******************************************************************************/

#define NSEC_PER_SEC (1000000000)                                                     		//nanoseconds per second
#define NSEC_PER_MSEC (1000000)                                             			//nanoseconds per milliseconds
#define NSEC_PER_USEC (1000)                                                            	//nanoseconds per microseconds
#define ERROR (-1)                                                                      	//error flag/variable set as -1
#define OK (0)                                                                         		//ok flag/variable set as 1
#define TEST_SECONDS (0)                                                             		//TEST_SECONDS variable set as 0
#define TEST_NANOSECONDS (NSEC_PER_MSEC * 10)                                  			//TEST_NANOSECONDS calculated using nanoseconds per milliseconds*10(test every 10 milliseonds)

/*********************************************************************Function Protoypes***********************************************************************/

void end_delay_test(void);                                                              	//prototype for end_delay_test function

/*********************************************************************Global variables*************************************************************************/

//structure to define the sleep time in terms of seconds and nanoseconds 
//syntax for the same is 
/*              struct timespec {
                  time_t  tv_sec;                                                       	//Seconds 
                  long    tv_nsec;                                                      	//Nanoseconds 
              };
*/
static struct timespec sleep_time = {0, 0};
static struct timespec sleep_requested = {0, 0};
static struct timespec remaining_time = {0, 0};

static unsigned int sleep_count = 0;                                                     	//sleep_count variable set to 0. counts the number of times the system goes into "nanosleep" mode. 
                                                                                         	//as soon as the value of this variable goes to 3, it loops back.

pthread_t main_thread;                                                                  	//main_thread declared as a pthread_t data type variable to uniquely identify the thread 
pthread_attr_t main_sched_attr;                                                  	 	//main_sched_attr thread is created using the pthread_attr_t thread creation attribute
int rt_max_prio, rt_min_prio, min;                                               	 	//integer variables rt_max_prio defines the maximum priority while rt_min_prio defines the minimum priority
struct sched_param main_param;                                                		 	//main param is a sched_param structure that describes the scheduling parameters


/********************************************************************Function definitions**********************************************************************/

/* function    : print_scheduler
*  params      : void
*  brief       : prints the scheduling policy used for the program
*  return type : void
*/
void print_scheduler(void)
{
   int schedType;                                                          		 	 //schedType defines the type of scheduling algorithm used.

   /*sched_getscheduler() returns the current scheduling policy of the thread identified by pid.  If pid equals zero, the policy of the calling thread will be retrieved.
    * A PID is a process ID. All the threads running on the same process will be associated with the same PID
    * getpid() returns the process ID (PID) of the calling process.*/
   
   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");                        		 	 //first-in, first-out policy
           break;
     case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n");                   		 	 //the standard round-robin time sharing policy
       break;
     case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n");                           		 	 //a round-robin policy
           break;
     default:										 	 //if pid is negative or pid of the thread could not be found
       printf("Pthread Policy is UNKNOWN\n");
   }
}

/* function	: d_Ftime
 * params	: struct timespec *fstart, struct timespec *fstop
 * brief	: finds the difference between the start and the stop time using the timespec struct pointers fstart and fstop
 * return type	: double
 * */
double d_ftime(struct timespec *fstart, struct timespec *fstop)
{
  double dfstart = ((double)(fstart->tv_sec) + ((double)(fstart->tv_nsec) / 1000000000.0));	 //adds nanoseconds precision to second value for start time
  double dfstop = ((double)(fstop->tv_sec) + ((double)(fstop->tv_nsec) / 1000000000.0));	 //adds nanoseconds precision to second value for stop time

  return(dfstop - dfstart); 									 //returns the difference between start and stop time 
}


/* function	: delta_t
 * params	: struct timespec *stop, struct timespec *start, struct timespec *delta_t
 * brief	: calculates the time in seconds and nanoseconds and returns the status 
 * return type	: int
 * */
int delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
  int dt_sec=stop->tv_sec - start->tv_sec;							 //calculates the time difference in seconds
  int dt_nsec=stop->tv_nsec - start->tv_nsec;							 //calculates the time difference in nanoseconds

  //printf("\ndt calcuation\n");

  // case 1 - less than a second of change							 //for change less than 1 second
  if(dt_sec == 0)										 //checks if the change in seconds is equal to 0
  {
             //printf("dt less than 1 second\n");

             if(dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)						 //if the change in nanoseconds is greater than 0 and less than that of a second
             {
                     //printf("nanosec greater at stop than start\n");
                        delta_t->tv_sec = 0;							 //tv_sec updated as 0 (delta_t timespec struct variable)
                        delta_t->tv_nsec = dt_nsec;						 //tv_nsec updated with the change in nanoseconds (delta_t timespec struct variable)
             }

             else if(dt_nsec > NSEC_PER_SEC)							 //if change in nanoseconds is greater than a second i.e. nanosecond overflow condition
             {
                     //printf("nanosec overflow\n");				
                        delta_t->tv_sec = 1;							 //set the tv_sec variable to 1 (delta_t timespec struct variable)
                        delta_t->tv_nsec = dt_nsec-NSEC_PER_SEC;				 //update the tv_nsec variable with the change in nanoseconds after subtracting a second
             }

             else // dt_nsec < 0 means stop is earlier than start				 //we already know for this section change in seconds is 0 (if condition satisfied) 
		     										 //hence change in nanoseconds less than 0 meaning the stop is earlier than start
												 //hence error generated
             {
                    printf("stop is earlier than start\n");
                      return(ERROR);  
             }
  }

  // case 2 - more than a second of change, check for roll-over
  else if(dt_sec > 0)                                                                            //if the change in time is more than a second
  {
             //printf("dt more than 1 second\n");

             if(dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)                                     	 //check for change in nanoseconds dt_sec , if greater than zero and less then a second       
             {
                     //printf("nanosec greater at stop than start\n");
                        delta_t->tv_sec = dt_sec;                                                //tv_sec updated with the time difference in seconds dt_sec 
                        delta_t->tv_nsec = dt_nsec;                                              //tv_nsec updated with the time difference in nanoseconds dt_nsec
             }

             else if(dt_nsec > NSEC_PER_SEC)                                                     //if the change in nanoseconds is greater than 1 second
             {
                     //printf("nanosec overflow\n");
                        delta_t->tv_sec = delta_t->tv_sec + 1;                                   //add 1 to tv_sec variable
                        delta_t->tv_nsec = dt_nsec-NSEC_PER_SEC;                                 //tv_nsec is updated with time in nanoseconds left after subtracting 1 second

             }

             else // dt_nsec < 0 means roll over                                                 //dt_nsec less than zero depicts roll over condition (value exceeded the range)
             {
                     //printf("nanosec roll over\n");
                        delta_t->tv_sec = dt_sec-1;                                              //subtract 1 from the difference 
                        delta_t->tv_nsec = NSEC_PER_SEC + dt_nsec;                               //add 1 second to dt_nsec and update tv_nsec with this value
             }
  }

  return(OK);											 //asserts that no error is generated 
}


//structure to define the sleep time in terms of seconds and nanoseconds for real time clock
static struct timespec rtclk_dt = {0, 0};
static struct timespec rtclk_start_time = {0, 0};
static struct timespec rtclk_stop_time = {0, 0};
static struct timespec delay_error = {0, 0};

//below variables are used to understand the behavior of clock_gettime function

//rtclk_code variables are used to get the time taken for execution from the start of the main loop to the end of the main loop
static struct timespec rtclk_code_diff = {0, 0};
static struct timespec rtclk_code_start_time = {0, 0};
static struct timespec rtclk_code_stop_time = {0, 0};

//rtclk_delay variables are used to get the time taken for execution from the start of the delay test function to the end of it
static struct timespec rtclk_delay_diff ={0,0};
static struct timespec rtclk_delay_start_time = {0, 0};
static struct timespec rtclk_delay_stop_time = {0, 0};

//time taken by code apart from delay tests
static struct timespec rtclk_rem_diff ={0,0};

//#define MY_CLOCK CLOCK_REALTIME								 //System-wide  clock  that  measures real (i.e., wall-clock) time. 
//#define MY_CLOCK CLOCK_MONOTONIC								 //Clock  that  cannot  be  set and represents monotonic time 
												 //since—as described by POSIX—"some unspecified point in the past".
#define MY_CLOCK CLOCK_MONOTONIC_RAW								 //Similar to CLOCK_MONOTONIC, but provides access to a raw hardware-based time that is not subject to NTP
												 //adjustments or the incremental adjustments performed by adjtime
//#define MY_CLOCK CLOCK_REALTIME_COARSE							 //A faster but less precise version of CLOCK_REALTIME.
//#define MY_CLOCK CLOCK_MONOTONIC_COARSE							 //A faster but less precise version of CLOCK_MONOTONIC. 

#define TEST_ITERATIONS (100)									 //number of test iterations to be performed defined as 100

/* function    : delay_test
 * params      : void *threadID
 * brief       : runs a loop for 100 iterations to test the system clock. For every iteration the test execution is delayed
 * 		 for the requester period of time(10 milliseconds). Calls delta_t and the end_delay_test functions.
 * return type : void 
 * */
void *delay_test(void *threadID)
{
   //store the realtime when the code starts to execute
   clock_gettime(MY_CLOCK,&rtclk_delay_start_time);

  int idx, rc;											 //idx variable used for test iterations
												 //rc variable stores the return value for the nanosleep function (can be 0 or -1)
  unsigned int max_sleep_calls=3;								 //maximum sleep calls allowed for the program are 3
  int flags = 0;				
  struct timespec rtclk_resolution;								 //stores the resoultion of the given clock (e.g. in terms of seconds, nanoseconds, milliseconds)

  sleep_count = 0;

  /*The  function  clock_getres()  finds the resolution (precision) of the specified clock clk_id, and, if res is non-NULL, stores it in the struct timespec pointed to by res. 
  The resolution of clocks depends on the implementation and cannot be configured by a particular process. Currently the resoultion is defined in terms of seconds, milliseconds and nanoseconds
  The res is an argument of timespec structure, as specified in <time.h>:
       struct timespec {
             time_t   tv_sec;        // seconds 
             long     tv_nsec;       // nanoseconds
         };
  clockid argument is the identifier of the particular clock on which to act.(for current programm clock realtime coarse option is selected)
  */

  //MY_CLOCK i.e. CLOCK_REALTIME_COARSE is passed into the getres function and it returns the resolution of the given clock
  if(clock_getres(MY_CLOCK, &rtclk_resolution) == ERROR) 					 //when clock_getres returns error
  {
      perror("clock_getres");										
      exit(-1);											 //exits the program
  }
  else												 //if no error generated and the function returns the resolution in terms of nanoseconds and seconds
  {
      printf("\n\nPOSIX Clock demo using system RT clock with resolution:\n\t%ld secs, %ld microsecs, %ld nanosecs\n", rtclk_resolution.tv_sec, (rtclk_resolution.tv_nsec/1000), rtclk_resolution.tv_nsec);
      //print the resolution of the clock in terms of seconds,microseconds and nanoseconds (microseconds calculated using ) 
  }

  for(idx=0; idx < TEST_ITERATIONS; idx++)							 //test runs for TEST_ITERATIONS i.e. 100 times
  {
      printf("test %d\n", idx);									 //prints the number of test iteration being performed

      /* run test for defined seconds */
      // updates the sleep_time struct with required number of seconds and nanoseconds the system is supposed to go into "nanosleep" mode
      sleep_time.tv_sec=TEST_SECONDS;								   
      sleep_time.tv_nsec=TEST_NANOSECONDS;
      sleep_requested.tv_sec=sleep_time.tv_sec;
      sleep_requested.tv_nsec=sleep_time.tv_nsec;

      /* start time stamp */ 
      // The functions clock_gettime() retrieves the time of the specified clock clk_id.
      // The clk_id argument is the identifier of the particular clock on which to act. 
      // A clock may be system-wide and hence visible for all processes, or per-process if it measures time only within a single process.

      clock_gettime(MY_CLOCK, &rtclk_start_time);						 //gets the realtime clock start_time

      /* request sleep time and repeat if time remains */
      do 
      {
	  /*nanosleep()  suspends  the execution of the calling thread until either at least
          * the time specified in *req has elapsed, or the delivery of a signal that
          * triggers the invocation of a handle in the calling thread or that terminates the process.
	  * on a successfully sleeping for the requested interval nanosleep returns 0 
	  * otherwise if interrupted by a handler (it stores the remaining time from the requested time into the remaining time struct) or error encountered, it returns -1.
	  */

          if(rc=nanosleep(&sleep_time, &remaining_time) == 0) break;			         //if the nanosleep executed for the requested period of time then break
          sleep_time.tv_sec = remaining_time.tv_sec;						 //sleep time updated with remaining time (seconds remaining)
          sleep_time.tv_nsec = remaining_time.tv_nsec;						 //sleep time updated with remaining time (nanoseconds remaining)
          sleep_count++;									 //sleep count incremented as the system did not sleep for the requested amount of time
      } 
      //the current loop runs until the system completes nanosleep for the requested time. If the system is interrupted during sleep
      //remaining time is stored in the variable and after the interrupt request is handled the system returns to the remaining time value
      //if this value is greater than zero the system again goes into nanosleep mode and completes sleep for the requested period of time 
      //NOTE: if the nanosleep is interrupted more than 3 times the loop execution is stopped
      while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0))				
                            && (sleep_count < max_sleep_calls));					

      clock_gettime(MY_CLOCK, &rtclk_stop_time);						 // retrieves the time of specified clock (here MY_CLOCK) 
												 //and stores it as stop time( tclk_stop_time- time after sleep)

      delta_t(&rtclk_stop_time, &rtclk_start_time, &rtclk_dt);					 //calculates the difference between the realtime start and realtime stop time 
      delta_t(&rtclk_dt, &sleep_requested, &delay_error);					 //calculates the delay error using the change in realtime start and realtime stop
      												 // and the requested nanosleep time (10ms, here requested sleep time is the calculates time)

      end_delay_test();										 //call to the end delay test function
  }

   //store the realtime when the code starts to execute
   clock_gettime(MY_CLOCK,&rtclk_delay_stop_time);

   delta_t(&rtclk_delay_stop_time, &rtclk_delay_start_time, &rtclk_delay_diff);                     //calculate the time difference between the start of code execution and the test completion
}

/* function    : end_delay_test
 * params      : void
 * brief       : prints the results of the delay test function
 * return type : void
 * */
void end_delay_test(void)
{
    double real_dt;                                                                              //stores the return value from the ftime function
#if 1
  printf("MY_CLOCK start seconds = %ld, nanoseconds = %ld\n", 
         rtclk_start_time.tv_sec, rtclk_start_time.tv_nsec);
  
  printf("MY_CLOCK clock stop seconds = %ld, nanoseconds = %ld\n", 
         rtclk_stop_time.tv_sec, rtclk_stop_time.tv_nsec);
#endif

//  real_dt=d_ftime(&rtclk_start_time, &rtclk_stop_time);					 //real_dt is updated with the difference between 
												 //the realtime clock start and realtime clock stop time
//  printf("MY_CLOCK clock DT seconds = %ld, msec=%ld, usec=%ld, nsec=%ld, sec=%6.9lf\n", 
//         rtclk_dt.tv_sec, rtclk_dt.tv_nsec/1000000, rtclk_dt.tv_nsec/1000, rtclk_dt.tv_nsec, real_dt);//prints the time in seconds, milliseconds, microseconds and nanoseconds

#if 1
  printf("Requested sleep seconds = %ld, nanoseconds = %ld\n", 
         sleep_requested.tv_sec, sleep_requested.tv_nsec);

//  printf("\n");
  printf("Sleep loop count = %d\n", sleep_count);
#endif

  real_dt=d_ftime(&rtclk_start_time, &rtclk_stop_time);                                          //real_dt is updated with the difference between 
                                                                                                 //the realtime clock start and realtime clock stop time
  printf("MY_CLOCK clock DT seconds = %ld, msec=%ld, usec=%ld, nsec=%ld, sec=%6.9lf\n",
         rtclk_dt.tv_sec, rtclk_dt.tv_nsec/1000000, rtclk_dt.tv_nsec/1000, rtclk_dt.tv_nsec, real_dt);//prints the time in seconds, milliseconds, microseconds and nanoseconds

  printf("MY_CLOCK delay error = %ld, nanoseconds = %ld\n", 
         delay_error.tv_sec, delay_error.tv_nsec);						 // prints the error obtained from the delay_test funtion
  printf("\n");
}

#define RUN_RT_THREAD										 //macro to define the run realtime thread variable

void main(void)
{
   int rc, scope;										 //rc variable in the main function stores 
												 //scope variable in the main function

   //store the realtime when the code starts to execute
   clock_gettime(MY_CLOCK,&rtclk_code_start_time);

   printf("Before adjustments to scheduling policy:\n");					 //before the schedulind policy is updated in the pthread attributes
   print_scheduler();

#ifdef RUN_RT_THREAD										 //if RUN_RT_THREAD 
   pthread_attr_init(&main_sched_attr);								 //initializing the main_sched_attribute values
   pthread_attr_setinheritsched(&main_sched_attr, PTHREAD_EXPLICIT_SCHED);			 //sets the main_sched_attr with PTHREAD_EXPLICIT_SCHED 
   pthread_attr_setschedpolicy(&main_sched_attr, SCHED_FIFO);					 //sets fifo scheduling as the scheduling policy for the ain_sched_attr

   rt_max_prio = sched_get_priority_max(SCHED_FIFO);						 //stores in rt_max_prio the maximum scheduling priority that can be set using fifo scheduling
   rt_min_prio = sched_get_priority_min(SCHED_FIFO);						 //stores in rt_min_prio the minimum scheduling priority that can be set using fifo scheduling

   
   main_param.sched_priority = rt_max_prio;							 //sets the scheduling pripority for main scheduling 
   rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);					 //sets the scheduler for main_param using the SCHED_FIFO


   if (rc)											 //if the functions returns an error, in a case like pid of the scheduling policy is not 
	   											 //known or the param is inadequate for the scheduling policy 
   {
       printf("ERROR; sched_setscheduler rc is %d\n", rc);					 //print error in scheduling and exit the program
       perror("sched_setschduler"); exit(-1);
   }

   printf("After adjustments to scheduling policy:\n");						 //once the pthread attributes are defined for the main scheduler and the scheduling policy is set
   print_scheduler();									         //print the current status of the scheduler

   main_param.sched_priority = rt_max_prio;							 //sets the scheduling parameters for the main_sched_attr using main_param
   pthread_attr_setschedparam(&main_sched_attr, &main_param);

   rc = pthread_create(&main_thread, &main_sched_attr, delay_test, (void *)0);			 //starts a new thread(main thread) in the calling process using the main_sched_attr
   												 //and the delay test function is to be executed, 0 is passed as the argument for start routine
   if (rc)											 //if the thread is not created 
   {
       printf("ERROR; pthread_create() rc is %d\n", rc);					 //print error message
       perror("pthread_create");
       exit(-1);										 //exit the program
   }

   pthread_join(main_thread, NULL);								 //waits for the main thread to end 	

   if(pthread_attr_destroy(&main_sched_attr) != 0)						 //if the main_sched_attr is not destroyed(once executed) 
     perror("attr destroy");									 //display error message
#else
   delay_test((void *)0);										
#endif

   printf("TEST COMPLETE\n");									 //test completion message

   //store the realtime when the code ends(all the test iterations are complete)
   clock_gettime(MY_CLOCK,&rtclk_code_stop_time);
   delta_t(&rtclk_code_stop_time, &rtclk_code_start_time, &rtclk_code_diff); 			 //calculate the time difference between the start of code execution and the test completion


   printf("Delay code starts to execute	: %ld sec, %ld nsec \nDelay code completes execution  : %ld sec, %ld nsec\n",
                                rtclk_delay_start_time.tv_sec, rtclk_delay_start_time.tv_nsec,rtclk_delay_stop_time.tv_sec, rtclk_delay_stop_time.tv_nsec);
   printf("Time taken for all the test iterations to complete: %ld sec %ld msec %ld usec %ld nsec\n",
                                rtclk_delay_diff.tv_sec, rtclk_delay_diff.tv_nsec/1000000, rtclk_delay_diff.tv_nsec/1000, rtclk_delay_diff.tv_nsec); 
   printf("Code starts to execute		: %ld sec, %ld nsec \nCode completes execution	: %ld sec, %ld nsec\n",
   				rtclk_code_start_time.tv_sec, rtclk_code_start_time.tv_nsec,rtclk_code_stop_time.tv_sec, rtclk_code_stop_time.tv_nsec);
   printf("Time taken for all the test iterations to complete: %ld sec %ld msec %ld usec %ld nsec\n",
				rtclk_code_diff.tv_sec, rtclk_code_diff.tv_nsec/1000000, rtclk_code_diff.tv_nsec/1000, rtclk_code_diff.tv_nsec); 

   delta_t(&rtclk_code_diff, &rtclk_delay_diff, &rtclk_rem_diff);
   printf("Time taken by code apart from delay test is: %ld sec %ld nsec\n", rtclk_rem_diff.tv_sec, rtclk_rem_diff.tv_nsec);
}

