#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *  Search through 'envs' for a runnable environment ,
 *  in circular fashion statrting after the previously running env,
 *  and switch to the first such environment found.
 *
 * Hints:
 *  The variable which is for counting should be defined as 'static'.
 */
void sched_yield(void)
{


 static int i=0;
 while(1){

	i=(i+1)%NENV;
	    if (envs[i].env_status == ENV_RUNNABLE)
       env_run(&envs[i]);
   }

/*
int i,j;
 
 if (curenv !=  NULL)
   i = curenv-envs;
 else
   i = 0;

 if (i == 0) {
   i++;  //  0 is the one we never want unless no choice.
 }

 //  Never even look at envs[0] here...
 for (j=i+1; j != i; j=((j%(NENV-1)) + 1))
   if (envs[j].env_status == ENV_RUNNABLE) {
     envs[j].env_runs += 1;
     env_run(&envs[j]);
     return;  //  errr
   }

 //  Stay in the same proc if none else was found
 if (envs[j].env_status == ENV_RUNNABLE) {
   env_run(&envs[j]); // stay in same environment
 } else if (envs[0].env_status == ENV_RUNNABLE) {
   //  Otherwise, switch back to idle process
   envs[0].env_runs += 1;
   env_run(&envs[0]); // switch to the idle process
 } else {
   //  Gah, idle process is busy!!
   panic("No environments left to run");
 }
*/
}
