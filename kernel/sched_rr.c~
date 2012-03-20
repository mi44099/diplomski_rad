/*! Round Robin Scheduler */
#define _KERNEL_

#include "sched_rr.h"
#include <kernel/sched.h>
#include <kernel/time.h>
#include <kernel/errno.h>
#include <lib/types.h>

static int rr_init ( ksched_t *self );
static int rr_thread_add ( kthread_t *thread );
static int rr_thread_remove ( kthread_t *thread );
static int rr_set_sched_parameters ( int sched_policy, sched_t *params );
static int rr_get_sched_parameters ( int sched_policy, sched_t *params );
static int rr_set_thread_sched_parameters ( kthread_t *kthread, sched_t *params );
static int rr_get_thread_sched_parameters ( kthread_t *kthread, sched_t *params );
static int rr_thread_activate ( kthread_t *kthread );
static void rr_timer ( void *p );
static int rr_thread_deactivate ( kthread_t *kthread );

/*! staticaly defined Round Robin Scheduler */
ksched_t ksched_rr = (ksched_t)
{
	.sched_id =		SCHED_RR,

	.init = 		rr_init,
	.thread_add =		rr_thread_add,
	.thread_remove =	rr_thread_remove,
	.thread_activate =	rr_thread_activate,
	.thread_deactivate =	rr_thread_deactivate,

	.set_sched_parameters =		rr_set_sched_parameters,
	.get_sched_parameters =		rr_get_sched_parameters,
	.set_thread_sched_parameters =	rr_set_thread_sched_parameters,
	.get_thread_sched_parameters =	rr_get_thread_sched_parameters,

	.params.rr.time_slice =	{ 0, 50000000 },
	.params.rr.threshold =	{ 0, 10000000 }
};

/*! Init RR scheduler */
static int rr_init ( ksched_t *self )
{
	*self = ksched_rr;

	/* reserve an empty alarm */
	self->params.rr.alarm.exp_time.sec = 0;
	self->params.rr.alarm.exp_time.nsec = 0;
	self->params.rr.alarm.period.sec = 0;
	self->params.rr.alarm.period.nsec = 0;
	self->params.rr.alarm.action = rr_timer;
	self->params.rr.alarm.param = NULL;
	self->params.rr.alarm.flags = 0;

	k_alarm_new ( &self->params.rr.rr_alarm, &self->params.rr.alarm,
		      KERNELCALL );

	return 0;
}

/*! Add thread to RR scheduler (give him initial time slice) */
static int rr_thread_add ( kthread_t *kthread )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
	ksched_t *gsched = ksched_get ( tsched->sched_policy );

	tsched->params.rr.remainder = gsched->params.rr.time_slice;

	return 0;
}
static int rr_thread_remove ( kthread_t *thread )
{
	/* TODO */
	return 0;
}

static int rr_set_sched_parameters ( int sched_policy, sched_t *params )
{
	ksched_t *gsched = ksched_get ( sched_policy );

	gsched->params.rr.time_slice = params->rr.time_slice;

	return 0;
}
static int rr_get_sched_parameters ( int sched_policy, sched_t *params )
{
	/* TODO */
	return 0;
}

static int rr_set_thread_sched_parameters ( kthread_t *kthread, sched_t *params )
{
	/* Nothing to set for thread! Just as an example - set to global RR */
	ksched_rr.params.rr.time_slice = params->rr.time_slice;

	return 0;
}

static int rr_get_thread_sched_parameters ( kthread_t *kthread, sched_t *params )
{
	/* TODO */
	return 0;
}

/*! Start time slice for thread (or countinue interrupted) */
static int rr_thread_activate ( kthread_t *kthread )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
	ksched_t *gsched = ksched_get ( tsched->sched_policy );

	/* check remainder if needs to be replenished */
	if ( time_cmp ( &tsched->params.rr.remainder,
		&gsched->params.rr.threshold ) <= 0 )
	{
		time_add ( &tsched->params.rr.remainder,
			   &gsched->params.rr.time_slice );
	}

	/* Get current time and store it */
	k_get_time ( &tsched->params.rr.slice_start );

	/* When to wake up? */
	tsched->params.rr.slice_end = tsched->params.rr.slice_start;
	time_add ( &tsched->params.rr.slice_end, &tsched->params.rr.remainder );

	/* Set alarm for remainder time */
	gsched->params.rr.alarm.exp_time = tsched->params.rr.slice_end;
	gsched->params.rr.alarm.param = kthread;

	k_alarm_set ( gsched->params.rr.rr_alarm, &gsched->params.rr.alarm );

	return 0;
}

/*! Timer interrupt for Round Robin */
static void rr_timer ( void *p )
{
	kthread_t *kthread = p;
	kthread_sched_data_t *tsched;

	if ( kthread_get_active () != kthread )
	{
		/* bug or rr thread got canceled! Let asume second :) */
		return;
	}

	tsched = kthread_get_sched_param ( kthread );

	/* given time is elapsed, set remainder to zero */
	tsched->params.rr.remainder.sec = tsched->params.rr.remainder.nsec = 0;

	/* move thread to ready queue - as last in coresponding queue */
	kthread_move_to_ready ( kthread, LAST );

	kthreads_schedule ();
}

/*!
 * Deactivate thread because:
 * 1. higher priority thread becomes active
 * 2. this thread time slice is expired
 * 3. this thread blocks on some queue
 */
static int rr_thread_deactivate ( kthread_t *kthread )
{
	/* Get current time and recalculate remainder */
	time_t t;
	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
	ksched_t *gsched = ksched_get ( tsched->sched_policy );

	if (tsched->params.rr.remainder.sec + tsched->params.rr.remainder.nsec)
	{
		/*
		 * "slice interrupted"
		 * recalculate remainder
		 */
		k_get_time ( &t );
		time_sub ( &tsched->params.rr.slice_end, &t );
		tsched->params.rr.remainder = tsched->params.rr.slice_end;

		if ( kthread_is_ready ( kthread ) )
		{
			/* is remainder too small or not? */
			if ( time_cmp ( &tsched->params.rr.remainder,
				&gsched->params.rr.threshold ) <= 0 )
			{
				kthread_move_to_ready ( kthread, LAST );
			}
			else {
				kthread_move_to_ready ( kthread, FIRST );
			}
		}
	}
	/* else = remainder is zero, thread is already enqueued in ready queue*/

	return 0;
}

