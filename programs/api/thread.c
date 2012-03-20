/*! Threads */

#include "thread.h"

#include <api/stdio.h>
#include <api/syscall.h>
#include <api/errno.h>
#include <lib/types.h>

/*! Thread creation/exit/wait/cancel ---------------------------------------- */

int create_thread ( void *start_func, void *param, int sched, int prio,
		    thread_t *handle )
{
	ASSERT_ERRNO_AND_RETURN ( start_func, E_INVALID_ARGUMENT );
	return syscall ( CREATE_THREAD, start_func, param, sched, prio, handle);
}

void thread_exit ( int status )
{
	syscall ( THREAD_EXIT, status );
}

int wait_for_thread ( void *thread, int wait )
{
	int retval;

	ASSERT_ERRNO_AND_RETURN ( thread, E_INVALID_ARGUMENT );

	do {
		retval = syscall ( WAIT_FOR_THREAD, thread, wait );
	}
	while ( retval == -E_RETRY && wait );

	return retval;
}

int cancel_thread ( void *thread )
{
	ASSERT_ERRNO_AND_RETURN ( thread, E_INVALID_ARGUMENT );
	return syscall ( CANCEL_THREAD, thread );
}

int thread_self ( thread_t *thread )
{
	ASSERT_ERRNO_AND_RETURN ( thread, E_INVALID_ARGUMENT );
	return syscall ( THREAD_SELF, thread );
}

/*! Start program */
int start_program ( char *prog_name, thread_t *handle, void *param,
		    int sched, int prio )
{
	ASSERT_ERRNO_AND_RETURN ( prog_name, E_INVALID_ARGUMENT );
	return syscall ( START_PROGRAM, prog_name, handle, param, sched, prio );
}

/*! Set thread scheduling parameters */
int set_sched_params ( thread_t *thread, int sched_policy, int prio,
		       sched_t *params )
{
	return syscall ( SET_SCHED_PARAMS, thread, sched_policy, prio, params );
}

/*! Get thread scheduling parameters */
int get_sched_params ( thread_t *thread, int *sched_policy, int *prio,
		       sched_t *params )
{
	return syscall ( GET_SCHED_PARAMS, thread, sched_policy, prio, params );
}

int set_thread_sched_params ( thread_t *thread, int sched_policy, int prio,
		       sched_t *params )
{
	return syscall ( SET_THREAD_SCHED_PARAMS, thread, sched_policy, prio, params );
}

int get_thread_sched_params ( thread_t *thread, int *sched_policy, int *prio,
		       sched_t *params )
{
	return syscall ( GET_THREAD_SCHED_PARAMS, thread, sched_policy, prio, params );
}


int edf_set ( time_t deadline, time_t period, int flags )
{
  sched_t param;
  thread_t thread;
  param.edf.deadline = deadline;
  param.edf.period = period;
  param.edf.flags = flags | EDF_SET ;

  thread_self ( &thread );

  return set_thread_sched_params ( &thread, SCHED_EDF, THR_DEFAULT_PRIO - 1, &param );
}

int edf_wait ( )
{
  sched_t param;
  thread_t thread;
  param.edf.flags = EDF_WAIT ;

  thread_self ( &thread );

  return set_thread_sched_params ( &thread, SCHED_EDF, THR_DEFAULT_PRIO - 1, &param );
}

int edf_exit ( )
{
  sched_t param;
  thread_t thread;
  param.edf.flags = EDF_EXIT ;

  thread_self ( &thread );

  return set_thread_sched_params ( &thread, SCHED_EDF, THR_DEFAULT_PRIO - 1, &param );
}
