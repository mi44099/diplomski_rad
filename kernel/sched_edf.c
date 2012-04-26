/*! EDF Scheduler */
#define _KERNEL_

//TODO:#include "sched_edf.h"

#include <kernel/sched.h>
#include <kernel/time.h>
#include <kernel/errno.h>
#include <lib/types.h>

static int edf_init ( ksched_t *self );
static int edf_thread_add ( kthread_t *thread );
static int edf_thread_remove ( kthread_t *thread );
static int edf_set_sched_parameters ( int sched_policy, sched_t *params );
static int edf_get_sched_parameters ( int sched_policy, sched_t *params );
static int edf_set_thread_sched_parameters(kthread_t *kthread, sched_t *params);
static int edf_get_thread_sched_parameters(kthread_t *kthread, sched_t *params);
static int edf_thread_activate ( kthread_t *kthread );
static void edf_timer ( void *p );
static int edf_thread_deactivate ( kthread_t *kthread );
static int k_edf_schedule ();
static int edf_check_deadline ( kthread_t *kthread );

/*! staticaly defined Earliest-Deadline-First Scheduler */
ksched_t ksched_edf = (ksched_t)
{
	.sched_id =		SCHED_EDF,

	.init = 		edf_init,
	.thread_add =		edf_thread_add,
	.thread_remove =	edf_thread_remove,
	.thread_activate =	edf_thread_activate,
	.thread_deactivate =	edf_thread_deactivate,

	.set_sched_parameters =		edf_set_sched_parameters,
	.get_sched_parameters =		edf_get_sched_parameters,
	.set_thread_sched_parameters =	edf_set_thread_sched_parameters,
	.get_thread_sched_parameters =	edf_get_thread_sched_parameters,
};

/*! Init EDF scheduler */
static int edf_init ( ksched_t *self )
{
	ASSERT ( self == &ksched_edf );

	self->params.edf.active = NULL;
	kthreadq_init ( &self->params.edf.ready );
	kthreadq_init ( &self->params.edf.wait );

	return 0;
}
static int edf_thread_add ( kthread_t *kthread )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );

	tsched->params.edf.edf_alarm = NULL;

	return 0;
}
static int edf_thread_remove ( kthread_t *kthread )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
	ksched_t *gsched = ksched_get ( tsched->sched_policy );

	if ( gsched->params.edf.active == kthread )
		gsched->params.edf.active = NULL;

	if ( tsched->params.edf.edf_alarm )
	{
		k_alarm_remove ( tsched->params.edf.edf_alarm );
		tsched->params.edf.edf_alarm = NULL;
	}

	tsched->sched_policy = SCHED_FIFO;

	k_edf_schedule ();

	return 0;
}
static int edf_set_sched_parameters ( int sched_policy, sched_t *params )
{
	return 0;
}
static int edf_get_sched_parameters ( int sched_policy, sched_t *params )
{
	return 0;
}

static int edf_arm_alarm ( kthread_t *kthread )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
	time_t now;
	alarm_t alarm;

	k_get_time ( &now );

	alarm.action = edf_timer;
	alarm.param = kthread;
	alarm.flags = ALARM_PERIODIC;
	alarm.period = tsched->params.edf.period;
	alarm.exp_time = tsched->params.edf.next_run;

	return k_alarm_new (	&tsched->params.edf.edf_alarm,
				&alarm,
				KERNELCALL );
}

static int edf_set_thread_sched_parameters (kthread_t *kthread, sched_t *params)
{
	time_t now;
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
//	ksched_t *gsched = ksched_get ( tsched->sched_policy );
	ksched_t *gsched = ksched_get ( SCHED_EDF );

	if ( gsched->params.edf.active == kthread )
		gsched->params.edf.active = NULL;

	k_get_time ( &now );

	if ( params->edf.flags == EDF_SET )
	{
		/*LOG( DEBUG, "%x [SET]", kthread );*/
		tsched->params.edf.period = params->edf.period;
		tsched->params.edf.relative_deadline = params->edf.deadline;
		tsched->params.edf.flags = params->edf.flags ^ EDF_SET;

		/* set periodic alarm */
		tsched->params.edf.next_run = now;
		time_add ( &tsched->params.edf.next_run, &params->edf.period );
		edf_arm_alarm ( kthread );

		/*
		 * adjust "next_run" and "deadline" for "0" period
		 * - first "edf_wait" will set correct values for first period
		 */
		tsched->params.edf.next_run = now;
		time_sub ( &tsched->params.edf.next_run, &params->edf.period );

		tsched->params.edf.active_deadline = now;
		time_add ( &tsched->params.edf.active_deadline,
			   &params->edf.deadline );
	}
	else if ( params->edf.flags == EDF_WAIT )
	{
		if ( edf_check_deadline ( kthread ) )
			return -1;

		/* set times for next period */
		time_add ( &tsched->params.edf.next_run,
			   &tsched->params.edf.period );

		tsched->params.edf.active_deadline = tsched->params.edf.next_run;
		time_add ( &tsched->params.edf.active_deadline,
			   &tsched->params.edf.relative_deadline );

		if ( kthread == gsched->params.edf.active )
			gsched->params.edf.active = NULL;

		/* TODO set (separate) alarm for deadline ?! */

		/* is task ready for execution, or must wait until next period */
		if ( time_cmp ( &tsched->params.edf.next_run, &now ) > 0 )
		{
			/* wait till "next_run" */
			LOG( DEBUG, "%x [EDF WAIT]", kthread );
			kthread_enqueue ( kthread, &gsched->params.edf.wait );
			kthreads_schedule (); /* will call edf_schedule() */
		}
		else {
			/* "next_run" has already come,
			 * activate task => move it to "EDF ready tasks"
			 */
			LOG( DEBUG, "%x [EDF READY]", kthread );
			LOG( DEBUG, "%x [1st READY]", kthreadq_get ( &gsched->params.edf.ready ) );
			kthread_enqueue ( kthread, &gsched->params.edf.ready );
			kthreads_schedule (); /* will call edf_schedule() */
		}
	}
	else if ( params->edf.flags == EDF_EXIT )
	{
		if ( kthread == gsched->params.edf.active )
			gsched->params.edf.active = NULL;

		//LOG( DEBUG, "%x [EXIT]", kthread );
		if ( edf_check_deadline ( kthread ) )
		{
			LOG( DEBUG, "%x [EXIT-error]", kthread );
			return -1;
		}

		LOG( DEBUG, "%x [EXIT-normal]", kthread );
		if ( tsched->params.edf.edf_alarm )
			k_alarm_remove ( tsched->params.edf.edf_alarm );

		tsched->sched_policy = SCHED_FIFO;

		LOG( DEBUG, "%x [EXIT]", kthread );
		if ( k_edf_schedule () )
		{
			LOG( DEBUG, "%x [EXIT]", kthread );

			kthreads_schedule (); /* will NOT call edf_schedule() */
		}

		LOG( DEBUG, "%x [EXIT]", kthread );
	}

	return 0;
}

static int edf_get_thread_sched_parameters (kthread_t *kthread, sched_t *params)
{
	return 0;
}

static int k_edf_schedule ()
{
	kthread_t *first, *next, *edf_active;
	kthread_sched_data_t *sch_first, *sch_next;
	ksched_t *gsched = ksched_get ( SCHED_EDF );
	int retval = 0;

	edf_active = gsched->params.edf.active;
	first = kthreadq_get ( &gsched->params.edf.ready );

	LOG( DEBUG, "%x [active]", edf_active );
	LOG( DEBUG, "%x [first]", first );
	//LOG( DEBUG, "%x [next]", next );

	if ( !first )
		return 0; /* no threads in edf.ready queue, edf.active unch. */

	if ( edf_active )
	{
		next = first;
		first = edf_active;
		LOG( DEBUG, "%x [next]", kthreadq_get_next ( next ) );
	}
	else {
		next = kthreadq_get_next ( first );
		LOG( DEBUG, "%x [next]", next );
	}

	while ( first && next )
	{

		sch_first = kthread_get_sched_param ( first );
		sch_next = kthread_get_sched_param ( next );

		if ( time_cmp ( &sch_first->params.edf.active_deadline,
			&sch_next->params.edf.active_deadline ) > 0 )
		{
			first = next;
		}

		next = kthreadq_get_next ( next );
	}

	if ( first && first != edf_active )
	{
		next = kthreadq_remove ( &gsched->params.edf.ready, first );
		LOG ( DEBUG, "%x removed, %x is now first", next, kthreadq_get ( &gsched->params.edf.ready ) );

		if ( edf_active )
		{
			LOG( DEBUG, "%x=>%x [EDF_SCHED_PREEMPT]",
			     edf_active, first );

			/*
			 * change active EDF thread:
			 * -remove it from active/ready list
			 * -put it into edf.ready list
			 */
			if ( kthread_is_ready (edf_active) )
			{
				if ( !kthread_is_active (edf_active) )
				{
					kthread_remove_from_ready (edf_active);
					
					/*
					 * set "deactivated" flag, don't need
					 * another call to "edf_schedule"
					 */
				}
				else {
					kthread_get_sched_param (edf_active)
						->activated = 0;
				}

				kthread_enqueue ( edf_active,
						  &gsched->params.edf.ready );
			}
			/* else = thread is blocked - leave it there */
		}

		gsched->params.edf.active = first;
		LOG( DEBUG, "%x [new active]", first );

		kthread_move_to_ready ( first, LAST );
		retval = 1;
	}

	return retval;
}

/*! Timer interrupt for edf */
static void edf_timer ( void *p )
{
	kthread_t *kthread = p, *test;

//	LOG( DEBUG, "%x [Alarm]", kthread );

	if ( kthread )
		test = kthreadq_remove ( &ksched_edf.params.edf.wait, kthread );

	LOG( DEBUG, "%x %x [Alarm]", kthread, test );

	if( kthread && test == kthread )
	{
		if ( edf_check_deadline ( kthread ) )
		{
			LOG( DEBUG, "%x [Alarm-xxx]", kthread );
			kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
			k_alarm_remove ( tsched->params.edf.edf_alarm );
			tsched->params.edf.edf_alarm = NULL;

			if ( !kthread_is_ready (kthread) )
			{
				LOG( DEBUG, "%x [Alarm-xxx]", kthread );
				kthread_set_syscall_retval ( kthread, -1 );
				kthread_move_to_ready ( kthread, LAST );

				kthreads_schedule ();
			}
		}
		else {
			//LOG( DEBUG, "%x [Alarm]", kthread );
			kthread_enqueue ( kthread, &ksched_edf.params.edf.ready );

			if ( k_edf_schedule () )
				kthreads_schedule ();
		}
	}
	else if ( kthread )
	{
		/*
		 * thread is not in edf.wait queue, but might be running or its
		 * blocked - its possible it missed deadline: check it
		 */
		if ( edf_check_deadline ( kthread ) )
		{
			/* what to do if its missed? kill thread? */
		}
	}
	else {
		/* TODO error? */
	}
}

static int edf_thread_activate ( kthread_t *kthread )
{
	return 0;
}

/*!
 * Deactivate thread because:
 * 1. higher priority thread becomes active
 * 2. this thread time slice is expired
 * 3. this thread blocks on some queue
 */
static int edf_thread_deactivate ( kthread_t *kthread )
{
	return k_edf_schedule ();
}

/*!
 * Check if task hasn't overrun its deadline at its start
 * Handle deadline overrun, based on flags
 */
static int edf_check_deadline ( kthread_t *kthread )
{
	/* TODO
	 * Check if "now" is greater than "active_deadline"
	 */

	time_t now;
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );

	k_get_time ( &now );

	if ( time_cmp ( &now, &tsched->params.edf.active_deadline ) > 0 )
	{
		LOG( DEBUG, "%x [DEADLINE OVERRUN]", kthread );
		return -1;
	}

	return 0;
}
