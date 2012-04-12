/*! EDF Scheduler */
#define _KERNEL_

//#include "sched_edf.h"

#include <kernel/sched.h>
#include <kernel/time.h>
#include <kernel/errno.h>
#include <lib/types.h>


static int edf_init ( ksched_t *self );
static int edf_thread_add ( kthread_t *thread );
static int edf_thread_remove ( kthread_t *thread );
static int edf_set_sched_parameters ( int sched_policy, sched_t *params );
static int edf_get_sched_parameters ( int sched_policy, sched_t *params );
static int edf_set_thread_sched_parameters ( kthread_t *kthread, sched_t *params );
static int edf_get_thread_sched_parameters ( kthread_t *kthread, sched_t *params );
static int edf_thread_activate ( kthread_t *kthread );
static void edf_timer ( void *p );
static int edf_thread_deactivate ( kthread_t *kthread );
static void k_edf_schedule ();

/*! staticaly defined Round Robin Scheduler */
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

	//.params.edf.time_slice =	{ 0, 50000000 },
	//.params.edf.threshold =	{ 0, 10000000 }
};

/*! Init edf scheduler */
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
static int edf_thread_remove ( kthread_t *thread )
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

static int edf_set_thread_sched_parameters ( kthread_t *kthread, sched_t *params )
{

	kthread_sched_data_t *tsched = kthread_get_sched_param ( kthread );
	ksched_t *gsched = ksched_get ( tsched->sched_policy );
	time_t now;
	alarm_t alarm;

	k_get_time ( &now );
	alarm.exp_time.sec = alarm.exp_time.nsec = 0;
	alarm.action = 0;
	alarm.param = 0;
	alarm.flags = 0;
	alarm.period.sec = alarm.period.nsec = 0;



	if ( params->edf.flags == EDF_SET )
	{
		kprint( " : %x \n", kthread );
		tsched->params.edf.period = params->edf.period;
		tsched->params.edf.deadline = params->edf.deadline;
		tsched->params.edf.flags = 0;//params->edf.flags ^ EDF_SET;

		tsched->params.edf.next_run = now;

		k_alarm_new (	&tsched->params.edf.edf_alarm,
				&alarm,
				KERNELCALL );
	
	}
	else if ( params->edf.flags == EDF_WAIT )
	{
		if ( time_cmp ( &tsched->params.edf.next_run, &now ) > 0 )
		{

			alarm.exp_time = tsched->params.edf.next_run;
			alarm.period.sec = alarm.period.nsec = 0;
			alarm.param = kthread;
			alarm.flags = 0;
			alarm.action = edf_timer;

			time_add (	&tsched->params.edf.next_run,
					&tsched->params.edf.period );

			kthread_enqueue ( kthread, &gsched->params.edf.wait );
			k_alarm_set ( tsched->params.edf.edf_alarm, &alarm );
		}
		else {
			time_add (	&tsched->params.edf.next_run,
					&tsched->params.edf.period );
			
			kthread_enqueue ( kthread, &gsched->params.edf.ready );

			k_edf_schedule ();
		}
	}
	else if ( params->edf.flags == EDF_EXIT )
	{
		//k_alarm_remove ( &tsched->params.edf.edf_alarm );

		tsched->sched_policy = SCHED_FIFO;

		// ovo ne treba, kad je aktivna, nije u edf.ready
		//(void) kthreadq_remove ( &gsched->params.edf.ready, kthread );

		kthread_remove_from_ready ( kthread );

		k_edf_schedule ();
	}

	return 0;
}

static int edf_get_thread_sched_parameters ( kthread_t *kthread, sched_t *params )
{
	return 0;
}

static void k_edf_schedule ()
{
	kthread_t *next, *first, *prva;
	kthread_sched_data_t *sch_first, *sch_next;
	ksched_t *gsched = ksched_get ( SCHED_EDF );
	time_t now, *prvi, *drugi;

//	k_get_time ( &now );

// ISPIS DRETVI U EDF.READY

prva = kthreadq_get ( &gsched->params.edf.ready );
kprint("\nREADY: ");
while( prva ) {
	kprint( ": %x ->", prva );
	prva = kthreadq_get_next ( prva );
}
kprint(" NULL!!\n");


	first = kthreadq_get ( &gsched->params.edf.ready );
//	kprint("\n prva u ready:%x\n", first);

	if ( first )
	{

		sch_first = kthread_get_sched_param ( first );
		gsched = ksched_get ( sch_first->sched_policy );

		next = kthreadq_get_next ( first );
//		kprint(" slijedeca u ready:%x \n", next);

		for ( ; next != NULL;  )
		{
			sch_next = kthread_get_sched_param ( next );

			prvi = (time_t *) (&sch_first->params.edf.next_run - &sch_first->params.edf.period + &sch_first->params.edf.deadline );	
			time_sub ( prvi, &now );

			drugi = (time_t *) (&sch_next->params.edf.next_run - &sch_next->params.edf.period + &sch_next->params.edf.deadline );
			time_sub ( drugi, &now );
kprint("\nprvi = %d, drugi = %d\n", prvi->sec, drugi->sec);

			if ( time_cmp (	prvi, drugi ) > 0 )
			{
				first = next;
			}
			next = kthreadq_get_next ( next );
		}

		ASSERT (first);

		(void) kthreadq_remove ( &gsched->params.edf.ready, first );

		kthread_move_to_ready ( first, LAST );
	}
/*
prva = kthreadq_get ( &gsched->params.edf.ready );
kprint("\nREADY: ");
while( prva ) {
	kprint( ": %x ->", prva );
	prva = kthreadq_get_next ( prva );
}
kprint(" NULL!!\n");
*/
	kthreads_schedule ();

}


static int edf_thread_activate ( kthread_t *kthread )
{
	return 0;
}

/*! Timer interrupt for edf */
static void edf_timer ( void *p )
{
	kthread_t *kthread = p;

	if ( kthread && kthreadq_remove ( &ksched_edf.params.edf.wait, kthread ) )       //!!!!!!!!!!!!!!
	{

		kprint("\n ---> ALARM : %x\n", kthread);

		kthread_enqueue ( kthread, &ksched_edf.params.edf.ready );
		k_edf_schedule ();
	}
}

/*!
 * Deactivate thread because:
 * 1. higher priority thread becomes active
 * 2. this thread time slice is expired
 * 3. this thread blocks on some queue
 */
static int edf_thread_deactivate ( kthread_t *kthread )
{

	if ( !kthread_is_active ( kthread ) )
		k_edf_schedule ();

	return 0;
}

