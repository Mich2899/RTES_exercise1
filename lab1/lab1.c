// Sam Siewert, September 2016
//
// Check to ensure all your CPU cores on in an online state.
//
// Check /sys/devices/system/cpu or do lscpu.
//
// Tegra is normally configured to hot-plug CPU cores, so to make all available,
// as root do:
//
// echo 0 > /sys/devices/system/cpu/cpuquiet/tegra_cpuquiet/enable
// echo 1 > /sys/devices/system/cpu/cpu1/online
// echo 1 > /sys/devices/system/cpu/cpu2/online
// echo 1 > /sys/devices/system/cpu/cpu3/online
//
// Check for precision time resolution and support with cat /proc/timer_list
//
// Ideally all printf calls should be eliminated as they can interfere with
// timing.  They should be replaced with an in-memory event logger or at least
// calls to syslog.

// This is necessary for CPU affinity macros in Linux
#define _GNU_SOURCE

#include <stdio.h>									//standard input output library
#include <stdlib.h>									//standard library
#include <unistd.h>									//unix standard library

#include <pthread.h>									//defines functions and APis for POSIX threads
#include <sched.h>									//defines sched_param for assigning scheduling parameters
#include <time.h>									//defines types, macros and functions for manipulating date and time
#include <semaphore.h>									//defines the sem_t type used for performing semaphore operations

#include <sys/sysinfo.h>								//defines functions that provide information regarding memory and swap
											//usage as well as the load average

#define USEC_PER_MSEC (1000)								//microseconds per milliseconds
#define NUM_CPU_CORES (1)								//CPU core number
#define FIB_TEST_CYCLES (100)								//the number of iterations to be performed for the FIB_TEST macro func
											//tion
#define NUM_THREADS (3)     								// service threads + sequencer
sem_t semF10, semF20;									//two semaphores for two services

#define FIB_LIMIT_FOR_32_BIT (47)							//the maximum limit for fibonacci sequence on a 32 bit system
#define FIB_LIMIT (10)									//numbr of iterations fibonacci calculations perfomed every iteration

int abortTest = 0;									//abort test is used to define the end of the sequencer test
double start_time;									//stores the very first instant when semaphore is provided to the thre
											//ad

unsigned int seqIterations = FIB_LIMIT;							//for every iterations there are seqIterations number of fibonacci cal
											//culations
unsigned int idx = 0, jdx = 1;								//varaibles for the fibonacci loop iterations
unsigned int fib = 0, fib0 = 0, fib1 = 1;						//variables for fibonacci series calculations

double getTimeMsec(void);								//prototype for getTimeMsec function


//macro function to calulate fibonacci sequence and run it for the required number of iterations to generate required delay
#define FIB_TEST(seqCnt, iterCnt)      \
   for(idx=0; idx < iterCnt; idx++)    \
   {                                   \
      fib0=0; fib1=1; jdx=1;           \
      fib = fib0 + fib1;               \
      while(jdx < seqCnt)              \
      {                                \
         fib0 = fib1;                  \
         fib1 = fib;                   \
         fib = fib0 + fib1;            \
         jdx++;                        \
      }                                \
   }                                   \


//below structure is used to define the index of the thread currently running and major periods
typedef struct
{
    int threadIdx;
    int MajorPeriods;
} threadParams_t;


/* Iterations, 2nd arg must be tuned for any given target type
   using timestamps
   
   Be careful of WCET overloading CPU during first period of LCM.
   
 */
void *fib10(void *threadp)
{
   double event_time, run_time=0.0;
   int limit=0, release=0, cpucore, i;
   threadParams_t *threadParams = (threadParams_t *)threadp;
   unsigned int required_test_cycles;

   // Assume FIB_TEST short enough that preemption risk is minimal
   //the fibonacci test function is executed for the first time to warm up the cache, the event_time is noted as the initial
   //i.e. before running the actual test to determine how much time do 100 iterations take and run_time is the time taken by the loop 
   //for execution
   FIB_TEST(seqIterations, FIB_TEST_CYCLES); //warm cache
   event_time=getTimeMsec();
   FIB_TEST(seqIterations, FIB_TEST_CYCLES);
   run_time=getTimeMsec() - event_time;

   //once the time taken by FIB_TEST to execute is determined we can execute it for calculated amount of iterations 
   //for the requested period of time, for this case it is 10 milliseconds 
   //requested_test_cycles determine the number of iterations for 10 milliseconds
   required_test_cycles = (int)(10.0/run_time);
   printf("F10 runtime calibration %lf msec per %d test cycles, so %u required\n", run_time, FIB_TEST_CYCLES, required_test_cycles);

   //while the test is not yet completed (abort test is set to 1 at the end of the sequencer function)
   while(!abortTest)
   {
       sem_wait(&semF10); 								//do not provide semaphore to the thread yet 

       if(abortTest)									//if abortTest is 1 break the loop
           break; 
       else 
           release++;									//else increment the release variable to depict the number of service 
       											//request being fulfilled for service 1

       cpucore=sched_getcpu();								//determines the number of CPU on which the calling thread is currentl
       											//y running
	
       //determine the current time at which the F10 service starts using the difference between the current time and when the execution started
       printf("F10 start %d @ %lf on core %d\n", release, (event_time=getTimeMsec() - start_time), cpucore);

       //the below loop is iterated for a requested period of 10 milliseconds
       //each FIB_TEST runs for a specific period of time and required_test_cycles is the number of times
       //the loop has to be iterated for a delay of 10 milliseconds
       do
       {
           FIB_TEST(seqIterations, FIB_TEST_CYCLES);
           limit++;
       }
       while(limit < required_test_cycles);

       //determine the current time at which the F10 service completes using the difference between the current time and when the execution started
       printf("F10 complete %d @ %lf, %d loops\n", release, (event_time=getTimeMsec() - start_time), limit);
       limit=0;
   }

   //terminates the calling thread and returns a value that is available to another thread in the same process that calls pthread_join
   pthread_exit((void *)0);
}

void *fib20(void *threadp)
{
   double event_time, run_time=0.0;
   int limit=0, release=0, cpucore, i;
   threadParams_t *threadParams = (threadParams_t *)threadp;
   int required_test_cycles;

   // Assume FIB_TEST short enough that preemption risk is minimal
   // the fibonacci test function is executed for the first time to warm up the cache, the event_time is noted 
   // as the initial i.e. before running the actual test to determine how much time do 100 iteratiosn take and 
   // run_time is the time taken by loop for execution. 
   FIB_TEST(seqIterations, FIB_TEST_CYCLES); //warm cache
   event_time=getTimeMsec();
   FIB_TEST(seqIterations, FIB_TEST_CYCLES);
   run_time=getTimeMsec() - event_time;

   //once the time taken by FIB_TEST to execute is determined we can execute it for calculated ampunt of iterations
   //for the requested period of time, for this case it is 20 milliseconds
   //requested_test_cycles determine the number of iterations for 20 milliseconds
   required_test_cycles = (int)(20.0/run_time);
   printf("F20 runtime calibration %lf msec per %d test cycles, so %d required\n", run_time, FIB_TEST_CYCLES, required_test_cycles);

   //while the etset is not yet completed abort test is set 1 at the end of the sequencer function)
   while(!abortTest)
   {
        sem_wait(&semF20);								//do not provide semaphore to the thread yet	

        if(abortTest)									//if abort_test is 1 break the loop
           break; 
        else 
           release++;									//else increment th release variable to depict the number of 
											//service request being fulfilled for service 2

        cpucore=sched_getcpu();								//determine the number of CPU on which the calling thread is currently
											//running
											
        printf("F20 start %d @ %lf on core %d\n", release, (event_time=getTimeMsec() - start_time), cpucore);

	//below loop is iterated for a period of 20 milliseconds
        do
        {
            FIB_TEST(seqIterations, FIB_TEST_CYCLES);
            limit++;
        }
        while(limit < required_test_cycles);

        printf("F20 complete %d @ %lf, %d loops\n", release, (event_time=getTimeMsec() - start_time), limit);
        limit=0;
   }

   pthread_exit((void *)0);
}


double getTimeMsec(void)
{
  struct timespec event_ts = {0, 0};

  //get the current time of the clock using clock_gettime fuction (the clock being used here is CLOCK_MONOTONIC)
  clock_gettime(CLOCK_MONOTONIC, &event_ts);
  return ((event_ts.tv_sec)*1000.0) + ((event_ts.tv_nsec)/1000000.0);			//return value in terms of milliseconds
}


void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
     case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
       break;
     case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n"); exit(-1);
           break;
     default:
       printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }

}


void *Sequencer(void *threadp)
{
  int i;
  int MajorPeriodCnt=0;
  double event_time;
  threadParams_t *threadParams = (threadParams_t *)threadp;

  printf("Starting Sequencer: [S1, T1=20, C1=10], [S2, T2=50, C2=20], U=0.9, LCM=100\n");
  start_time=getTimeMsec();
	
  //start_time is the time before the sequencer schedule is executed. This time is noted even before the calibration for the fib10 and fib20 functions 
  //is performed.

  // Sequencing loop for LCM phasing of S1, S2
  do
  {

      // Basic sequence of releases after CI for 90% load
      //
      // S1: T1= 20, C1=10 msec 
      // S2: T2= 50, C2=20 msec
      //
      // This is equivalent to a Cyclic Executive Loop where the major cycle is
      // 100 milliseconds with a minor cycle of 20 milliseconds, but here we use
      // pre-emption rather than a fixed schedule.
      //
      // Add to see what happens on edge of overload
      // T3=100, C3=10 msec -- add for 100% utility
      //
      // Use of usleep is not ideal, but is sufficient for predictable response.
      // 
      // To improve, use a real-time clock (programmable interval time with an
      // ISR) which in turn raises a signal (software interrupt) to a handler
      // that performs the release.
      //
      // Better yet, code a driver that direction interfaces to a hardware PIT
      // and sequences between kernel space and user space.
      //
      // Final option is to write all real-time code as kernel tasks, more like
      // an RTOS such as VxWorks.
      //

      // Simulate the C.I. for S1 and S2 and timestamp in log
      // Critical instant calculated
      printf("\n**** CI t=%lf\n", event_time=getTimeMsec() - start_time);
      //provide semaphores to the threads so as to execute both the services but according to their priority (F10 has higher priority)
      sem_post(&semF10);printf("semF10====\n"); sem_post(&semF20);printf("semF20====\n");

      //suspend the execution of the onging thread for an interval of 20 milliseconds and 
      //then provide semaphore for service 1 
      usleep(20*USEC_PER_MSEC); sem_post(&semF10);printf("semF10====\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      //suspend the execution of the ongoing thread for an interval of 20 milliseconds and
      //then provide semaphore for service 1
      usleep(20*USEC_PER_MSEC); sem_post(&semF10);printf("semF10====\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      //suspend the execution of the ongoing thread for an interval of 10 milliseconds and 
      //then provide sempahore for service 2
      usleep(10*USEC_PER_MSEC); sem_post(&semF20);printf("semF20====\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      //suspend the execution of the ongoing thread for an interval of 10 milliseconds and
      //then provide the semaphore for service 1
      usleep(10*USEC_PER_MSEC); sem_post(&semF10);printf("semF10====\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      //suspend the execution of the ongoing thread for an interval of 20 milliseconds and 
      //then provide the semaphore for service 1
      usleep(20*USEC_PER_MSEC); sem_post(&semF10);printf("semF10====\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      //suspend the execution of ongoing threads for 20 milliseconds
      usleep(20*USEC_PER_MSEC);

      //increment the value of MajorPeriodCnt
      MajorPeriodCnt++;
   } 
   while (MajorPeriodCnt < threadParams->MajorPeriods); 				//the loop executes until the count reaches the MajorPeriods value of 											      //thread params structure
 
   abortTest=1;										//abort the sequencer
   sem_post(&semF10); sem_post(&semF20);						//unlock the semaphores for both the services
}


void main(void)
{
    int i, rc, scope;
    cpu_set_t threadcpu;								//data structure that represents a set of CPUs
    pthread_t threads[NUM_THREADS];							//an array of thread identifiers
    threadParams_t threadParams[NUM_THREADS];						//an array of threadParams_t structure
    pthread_attr_t rt_sched_attr[NUM_THREADS];						//an array of thread attribute creation atrribute structures
    int rt_max_prio, rt_min_prio;
    struct sched_param rt_param[NUM_THREADS];						//sched_param structure array
    struct sched_param main_param;							//sched_param structure for the main thread
    pthread_attr_t main_attr;								//thread creation attribute for the main thread
    pid_t mainpid;									//process id type for the main thread
    cpu_set_t allcpuset;								
			
    abortTest=0;									//abort test set to zero for the sequencer to run

   printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());

   //clear the set so that it contains no CPUs
   CPU_ZERO(&allcpuset);

   //add cpu to set 
   for(i=0; i < NUM_CPU_CORES; i++)
       CPU_SET(i, &allcpuset);

   //return the number of CPUS in set
   printf("Using CPUS=%d from total available.\n", CPU_COUNT(&allcpuset));


    // initialize the sequencer semaphores
    // sem_init() initializes the unnamed semaphore at the address pointed to by sem.  The value argument specifies the initial value for the semaphore.
    // The pshared argument indicates whether this semaphore is to be shared between the threads of a process, or between processes.
    // If pshared has the value 0, then the semaphore is shared between the threads of a process, and should be located at some address that is visible to
    // all threads (e.g., a global variable, or a variable allocated dynamically on the heap).

    if (sem_init (&semF10, 0, 0)) { printf ("Failed to initialize semF10 semaphore\n"); exit (-1); }
    if (sem_init (&semF20, 0, 0)) { printf ("Failed to initialize semF20 semaphore\n"); exit (-1); }

    //get the process id for main thread
    mainpid=getpid();

    //set the maximum and minimum prioity into the variables for fifo scheduling
    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    //get the parameters for the main thread and set the priority for it
    rc=sched_getparam(mainpid, &main_param);
    main_param.sched_priority=rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);		//set the schdeular for the current thread specified by getpid()
    if(rc < 0) perror("main_param");					//error in setting the scheduler
    print_scheduler();							//print current status of the scheduler


    pthread_attr_getscope(&main_attr, &scope);				//sets contention scope attribute of the thread_attributes object referred to by 
    									//main_attr

    //print the scope of the thread 
    if(scope == PTHREAD_SCOPE_SYSTEM)
      printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
      printf("PTHREAD SCOPE PROCESS\n");
    else
      printf("PTHREAD SCOPE UNKNOWN\n");

    //print maximum and minimum priority for the given scheduling policy
    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    //set the thread attributes for all the threads in the process
    for(i=0; i < NUM_THREADS; i++)
    {

      CPU_ZERO(&threadcpu);
      CPU_SET(3, &threadcpu);

      rc=pthread_attr_init(&rt_sched_attr[i]);						//initialize the scheduler attributes with default value
      rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);	//set the inherit-scheduler attribute of the thread attributes
      											//object by PTHREAD_EXPLICIT_SCHED
      rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);			//scheduling policy set as fifo
      rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);	//cpu affinity mask of thread created using the thread attribute

      rt_param[i].sched_priority=rt_max_prio-i;
      pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);			//set the scheduling parameters

      threadParams[i].threadIdx=i;							//thread index
    }
   
    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    // Create Service threads which will block awaiting release for:
    //
    // serviceF10
    rc=pthread_create(&threads[1],               // pointer to thread descriptor
                      &rt_sched_attr[1],         // use specific attributes
                      //(void *)0,                 // default attributes
                      fib10,                     // thread function entry point
                      (void *)&(threadParams[1]) // parameters to pass in
                     );
    // serviceF20
    rc=pthread_create(&threads[2],               // pointer to thread descriptor
                      &rt_sched_attr[2],         // use specific attributes
                      //(void *)0,                 // default attributes
                      fib20,                     // thread function entry point
                      (void *)&(threadParams[2]) // parameters to pass in
                     );


    // Wait for service threads to calibrate and await relese by sequencer
    usleep(300000);
 
    // Create Sequencer thread, which like a cyclic executive, is highest prio
    printf("Start sequencer\n");
    threadParams[0].MajorPeriods=3;

    rc=pthread_create(&threads[0],               // pointer to thread descriptor
                      &rt_sched_attr[0],         // use specific attributes
                      //(void *)0,                 // default attributes
                      Sequencer,                 // thread function entry point
                      (void *)&(threadParams[0]) // parameters to pass in
                     );


    //wait until all the threads are destroyed
   for(i=0;i<NUM_THREADS;i++)
       pthread_join(threads[i], NULL);

  //test completion message
   printf("\nTEST COMPLETE\n");

}
             
