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
	self->params.edf.active = NULL;
	kthreadq_init ( &self->params.edf.ready );
	kthreadq_init ( &self->params.edf.wait );

	return 0;
}
static int edf_thread_add ( kthread_t *kthread )
{
	return 0;
}
static int edf_thread_remove ( kthread_t *kthread )
{
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

static int edf_set_thread_sched_parameters (kthread_t *kthread, sched_t *params)
{
	time_t now;
	alarm_t alarm;
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
	ksched_t *gsched = ksched_get ( tsched->sched_policy );

	k_get_time ( &now );

	if ( params->edf.flags == EDF_SET )
	{
		/* LOG( DEBUG, "%x [SET]\n", kthread ); --OVAKO DEBUGIRATI */
		tsched->params.edf.period = params->edf.period;
		tsched->params.edf.relative_deadline = params->edf.deadline;
		tsched->params.edf.flags = params->edf.flags ^ EDF_SET;

		/*
		 * adjust "next_run" and "deadline" for "0" period
		 * - first "wait" will set correct values for first period
		 */
		tsched->params.edf.next_run = now;
		time_sub ( &tsched->params.edf.next_run, &params->edf.period );

		tsched->params.edf.active_deadline = now;
		time_add ( &tsched->params.edf.active_deadline,
			   &params->edf.deadline );

		/* set periodic alarm, first activation: now+period (2nd p.) */
		alarm.action = edf_timer;
		alarm.param = kthread;
		alarm.flags = ALARM_PERIODIC;
		alarm.period = tsched->params.edf.period;
		alarm.exp_time = now;
		time_add ( &alarm.exp_time, &alarm.period );

		k_alarm_new (	&tsched->params.edf.edf_alarm,
				&alarm,
				KERNELCALL );
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

		/* TODO set (separate) alarm for deadline ?! */

		/* is task ready for execution, or must wait until next period */
		if ( time_cmp ( &tsched->params.edf.next_run, &now ) > 0 )
		{
			/* wait till "next_run" */
			kthread_enqueue ( kthread, &gsched->params.edf.wait );
			kthreads_schedule (); /* will call edf_schedule() */
			LOG( DEBUG, "%x [WAIT]", kthread );
		}
		else {
			/* "next_run" has already come,
			 * activate task => move it to "EDF ready tasks"
			 */
			kthread_enqueue ( kthread, &gsched->params.edf.ready );
			kthreads_schedule (); /* will call edf_schedule() */
			LOG( DEBUG, "%x [WAIT]", kthread );
		}
	}
	else if ( params->edf.flags == EDF_EXIT )
	{
		if ( edf_check_deadline ( kthread ) )
			return -1;

		k_alarm_remove ( tsched->params.edf.edf_alarm );

		tsched->sched_policy = SCHED_FIFO;

		if ( k_edf_schedule () )
			kthreads_schedule (); /* will NOT call edf_schedule() */
	}

	return 0;
}

static int edf_get_thread_sched_parameters (kthread_t *kthread, sched_t *params)
{
	return 0;
}

static int k_edf_schedule ()
{
	kthread_t *first, *next;
	kthread_sched_data_t *sch_first, *sch_next;
	ksched_t *gsched = ksched_get ( SCHED_EDF );
	int retval = 0;

	next = first = kthreadq_get ( &gsched->params.edf.ready );

	while ( first != NULL && (next = kthreadq_get_next ( next )) != NULL )
	{

		sch_first = kthread_get_sched_param ( first );
		sch_next = kthread_get_sched_param ( next );

		if ( time_cmp ( &sch_first->params.edf.active_deadline,
			&sch_next->params.edf.active_deadline ) > 0 )
		{
			first = next;
		}
	}

	if ( first )
	{
		(void) kthreadq_remove ( &gsched->params.edf.ready, first );
		kthread_move_to_ready ( first, LAST );
		retval = 1;
	}

	return retval;
}

/*! Timer interrupt for edf */
static void edf_timer ( void *p )
{
	kthread_t *kthread = p;

	LOG( DEBUG, "%x [Alarm]", kthread );

	if( kthread && kthreadq_remove ( &ksched_edf.params.edf.wait, kthread ))
	{
		if ( edf_check_deadline ( kthread ) )
			return;

		LOG( DEBUG, "%x [Alarm]", kthread );
		kthread_enqueue ( kthread, &ksched_edf.params.edf.ready );

		k_edf_schedule ();
		kthreads_schedule ();
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
	return 0;
}