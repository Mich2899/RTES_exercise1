/****************************************************************************/
/* pthread.c                                                                */
/*                                                                          */
/* Sam Siewert - 02/05/2011                                                 */
/* Edited by Michelle Christian                                             */
/****************************************************************************/

/*********************************************************************************Includes**************************************************************************/
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

/**********************************************************************************Macros**************************************************************************/
#define NUM_THREADS 64                                                                  //the total number of threads to be created and processed

/*****************************************************************************Global Variables********************************************************************/
typedef struct
{
    int threadIdx;                                                                      //structure that contains index as an element
} threadParams_t;


// POSIX thread declarations and scheduling attributes
pthread_t threads[NUM_THREADS];                                                         //An array of threads is declared with the name threads with size NUM_THREADS
threadParams_t threadParams[NUM_THREADS];                                               //an array of structure objects is created with name threadParams and size NUM_THREADS


/**************************************************************************Function Definitions*******************************************************************/
/* function     : counterThread
 * params       : void *threadp
 * brief        : takes the threadParam structure as input and processes
 *                the index element of the structure to calculate the sum 
 *                of value upto the index. Prints the thread index and the 
 *                sum of values upto that index starting from 1
 * return type  : void 
 * */
void *counterThread(void *threadp)
{
    int sum=0, i;                                                                       //sum to calculate the sum of values upto the index of thread
    threadParams_t *threadParams = (threadParams_t *)threadp;                           //dereferencing the void pointer into threadParams_t type
    for(i=1; i < (threadParams->threadIdx)+1; i++)                                      //for loop runs from 1 up-to the index of the structure
        sum=sum+i;                                                                      //update sum 

    printf("Thread idx=%d, sum[1...%d]=%d\n",                                           //print the Thread index and sum up-to that index
           threadParams->threadIdx,
           threadParams->threadIdx, sum);
}


int main (int argc, char *argv[])
{
   int rc;
   int i;                                                                               //integer i declareration, used in for loop

   for(i=1; i <= NUM_THREADS; i++)                                                      //runs from 1 to NUM_THREADS(64)        
   {
       threadParams[i].threadIdx=i;
       pthread_create(&threads[i],                                                      // pointer to thread descriptor
                      (void *)0,                                                        // use default attributes
                      counterThread,                                                    // thread function entry point
                      (void *)&(threadParams[i])                                        // parameters to pass in
                     );
   }

   for(i=0;i<NUM_THREADS;i++)								//run the loop for all the threads to end 
       pthread_join(threads[i], NULL);							

   printf("TEST COMPLETE\n");								//TEST completion message
}


