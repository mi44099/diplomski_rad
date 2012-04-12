/*! Secondary scheduler general functions */
#define _KERNEL_

#include "sched.h"
#include <kernel/errno.h>

extern ksched_t ksched_rr;
extern ksched_t ksched_edf;

/*! Staticaly defined schedulers (could be easily extended to dynamicaly) */
static ksched_t *ksched[] = {
	NULL,		/* SCHED_FIFO */
	&ksched_rr,	/* SCHED_RR */
	&ksched_edf	/* SCHED_EDF */
};

/*! Get pointer to ksched_t parameters for requested scheduling policy */
ksched_t *ksched_get ( int sched_policy )
{
	ASSERT ( sched_policy >= 0 && sched_policy < SCHED_NUM );

	return ksched[sched_policy];
}

/*! Initialize all (known) schedulers (called from 'kthreads_init') */
void ksched_init ()
{
	int i;

	for ( i = 0; i < SCHED_NUM; i++ )
		if ( ksched[i] && ksched[i]->init )
			ksched[i]->init( ksched[i] );
}

/*! Set (change) scheduling policy for existing thread (or just created) */
int ksched_set_thread_policy ( kthread_t *kthread, int new_policy )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param (kthread);
	int old_policy = tsched->sched_policy;

	ASSERT ( old_policy >= 0 && old_policy < SCHED_NUM );
	ASSERT ( new_policy >= 0 && new_policy < SCHED_NUM );

	if ( ksched[old_policy] && ksched[old_policy]->thread_remove )
		ksched[old_policy]->thread_remove ( kthread );

	tsched->sched_policy = new_policy;

	if ( ksched[new_policy] && ksched[new_policy]->thread_add )
		ksched[new_policy]->thread_add ( kthread );

	return old_policy;
}

/*! Add thread to scheduling policy (if required by policy) */
int ksched_thread_add ( kthread_t *kthread, int sched_policy )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param (kthread);

	ASSERT ( sched_policy >= 0 && sched_policy < SCHED_NUM );

	tsched->sched_policy = sched_policy;
	tsched->activated = 0;

	if ( ksched[sched_policy] && ksched[sched_policy]->thread_add )
		ksched[sched_policy]->thread_add ( kthread );

	return 0;
}
/*! Remove thread from scheduling policy (if required by policy) */
int ksched_thread_remove ( kthread_t *kthread, int sched_policy )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param (kthread);

	ASSERT ( sched_policy >= 0 && sched_policy < SCHED_NUM );

	ASSERT ( tsched->sched_policy == sched_policy );

	if ( ksched[sched_policy] && ksched[sched_policy]->thread_remove )
		ksched[sched_policy]->thread_remove ( kthread );

	return 0;
}

/*! Actions to be performed when thread is to become active */
int ksched_activate_thread ( kthread_t *kthread )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param (kthread);
	int activated = tsched->activated;
	int sched = tsched->sched_policy;

	if ( activated == 0 )
	{
		if ( ksched[sched] && ksched[sched]->thread_activate )
			ksched[sched]->thread_activate ( kthread );

		tsched->activated = 1;
	}

	return activated;
}

/*! Actions to be performed when thread is removed as active */
int ksched_deactivate_thread ( kthread_t *kthread )
{
	kthread_sched_data_t *tsched = kthread_get_sched_param (kthread);
	int activated = tsched->activated;
	int sched = tsched->sched_policy;

	if ( activated == 1 )
	{
		if ( ksched[sched] && ksched[sched]->thread_deactivate )
			ksched[sched]->thread_deactivate ( kthread );

		tsched->activated = 0;
	}

	return activated;
}


/*! Interface to threads ---------------------------------------------------- */

/*! Set thread scheduling parameters */
int sys__set_thread_sched_params ( void *p )
{
	//parameters on thread stack
	thread_t *thread;
	int sched_policy;
	int prio;
	sched_t *params;
	//other variables
	kthread_t *kthread;

	thread = *( (void **) p ); p += sizeof (void *);
	thread = U2K_GET_ADR ( thread, kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( thread && thread->thread, E_INVALID_HANDLE );
	kthread = thread->thread;
	ASSERT_ERRNO_AND_EXIT ( thread->thr_id == kthread_get_id ( kthread ),
				E_INVALID_HANDLE );

	sched_policy = *( (int *) p ); p += sizeof (int);
	ASSERT_ERRNO_AND_EXIT ( sched_policy > 0 && sched_policy < SCHED_NUM,
			       E_INVALID_HANDLE );

	prio = *( (int *) p ); p += sizeof (int);
	ASSERT_ERRNO_AND_EXIT ( prio >= 0 && prio < PRIO_LEVELS,
			       E_INVALID_HANDLE );

	params = *( (void **) p ); p += sizeof (void *);
	params = U2K_GET_ADR ( params, kthread_get_process (NULL) );

	/* set new scheduling parameters */
	ksched_set_thread_policy ( kthread, sched_policy );

	if ( prio > 0 )
		kthread_set_prio ( kthread, prio );

	if ( params && ksched[sched_policy] &&
	     ksched[sched_policy]->set_thread_sched_parameters )
		ksched[sched_policy]->set_thread_sched_parameters ( kthread,
								    params );

	return 0;
}

/*! Get thread scheduling parameters */
int sys__get_thread_sched_params ( void *p )
{
	//parameters on thread stack
	thread_t *thread;
	int *sched_policy;
	int *prio;
	sched_t *params;
	//other variables
	kthread_t *kthread;
	kthread_sched_data_t *tsched;

	thread = *( (void **) p ); p += sizeof (void *);
	thread = U2K_GET_ADR ( thread, kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( thread && thread->thread, E_INVALID_HANDLE );
	kthread = thread->thread;
	ASSERT_ERRNO_AND_EXIT ( thread->thr_id == kthread_get_id ( kthread ),
				E_INVALID_HANDLE );

	/* get scheduling parameters */
	tsched = kthread_get_sched_param (kthread);

	sched_policy = *( (int **) p ); p += sizeof (int *);
	if ( sched_policy )
	{
		sched_policy = U2K_GET_ADR ( sched_policy,
					     kthread_get_process (NULL) );
		if ( sched_policy )
			*sched_policy = tsched->sched_policy;
	}

	prio = *( (int **) p ); p += sizeof (int *);
	if ( prio != NULL )
	{
		prio = U2K_GET_ADR ( prio, kthread_get_process (NULL) );

		if ( prio )
			*prio = kthread_get_prio ( kthread );
	}

	params = *( (void **) p ); p += sizeof (void *);
	if ( params )
	{
		params = U2K_GET_ADR ( params, kthread_get_process (NULL) );

		if ( params && ksched[tsched->sched_policy] &&
		     ksched[tsched->sched_policy]->get_thread_sched_parameters )
			ksched[tsched->sched_policy]->
				get_thread_sched_parameters ( kthread, params );
	}

	return 0;
}

/*! Set scheduling parameters */
int sys__set_sched_params ( void *p )
{
	//parameters on thread stack
	int sched_policy;
	sched_t *params;

	sched_policy = *( (int *) p ); p += sizeof (int);
	ASSERT_ERRNO_AND_EXIT ( sched_policy > 0 && sched_policy < SCHED_NUM,
			       E_INVALID_HANDLE );

	params = *( (void **) p ); p += sizeof (void *);
	params = U2K_GET_ADR ( params, kthread_get_process (NULL) );

	/* set new scheduling parameters */
	if ( params && ksched[sched_policy] &&
	     ksched[sched_policy]->set_sched_parameters )
		ksched[sched_policy]->set_sched_parameters ( sched_policy,
							     params );
	return 0;
}
/*! Get scheduling parameters */
int sys__get_sched_params ( void *p )
{
	/* TODO */
	return 0;
}
