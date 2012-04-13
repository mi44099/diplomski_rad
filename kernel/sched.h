/*! Secondary scheduler interface */

/*!
 * Native scheduler implemented in kernel is priority scheduler with FIFO for
 * threads with same priority. Secondary schedulers can influnce scheduling by
 * adjusting priority of tasks (threads) they are "scheduling".
 * For more information on how to implement particular scheduler look at example
 * given with Round Robin scheduling (sched_rr.h/c).
 */

#pragma once

#ifdef _KERNEL_

union _kthread_sched_params_t_;
struct _kthread_sched_data_t_;

typedef union _kthread_sched_params_t_ kthread_sched_params_t;
typedef struct _kthread_sched_data_t_ kthread_sched_data_t; /* included in kthread descriptor */
#include <kernel/thread.h>
#include <lib/types.h>
#include <kernel/sched_rr.h>
#include <kernel/sched_edf.h>

/*! Thread specific data/interface ------------------------------------------ */

/*! Union of per thread specific data types required by all schedulers */
union _kthread_sched_params_t_
{
	ksched_rr_thread_params rr;	/* Round Robin per thread data */
	ksched_edf_thread_params_t edf;

	/* add others thread scheduling parameters for other schedulers that
	   require parameters */
};

/*! Scheduling parameters for each thread (included in thread descriptor) */
struct _kthread_sched_data_t_
{
	//int prio;		/* thread priority (current priority) */
	//priority is directly included in thread descriptor, since primary
	//scheduler uses priority

	int sched_policy;	/* scheduling policy */

	int activated;		/* disable multiple activation/deactivation
				   calls when thread becomes active (or stop
				   being active) */

	kthread_sched_params_t params;	/* scheduler per thread specific data */
};
 /* included in kthread descriptor */



int ksched_set_thread_policy ( kthread_t *kthread, int new_policy );

int ksched_thread_add ( kthread_t *kthread, int sched_policy );
/*int ksched_thread_remove ( kthread_t *kthread, int sched_policy );*/
int ksched_thread_remove ( kthread_t *kthread );

int ksched_activate_thread ( kthread_t *kthread );
int ksched_deactivate_thread ( kthread_t *kthread );


/*! Global scheduler specific data/interface -------------------------------- */

/*! Union of per scheduler specific data types required */
typedef union _ksched_params_t_
{
	ksched_rr_t rr;	/* Round Robin global data */
	ksched_edf_t edf;

	/* add others thread scheduling parameters for other schedulers that
	   require parameters */
}
ksched_params_t;

/*! Secondary scheduler interface */
struct _ksched_t_;
typedef struct _ksched_t_ ksched_t;
struct _ksched_t_
{
	int sched_id;	/* scheduler ID, e.g. SCHED_FIFO */

	int (*init) ( ksched_t *self );	/* init scheduler */

	/* actions when thread is created or when it switch to this scheduler */
	int (*thread_add) ( kthread_t * );

	/* actions when thread is created or when it switch to this scheduler */
	int (*thread_remove) ( kthread_t * );

	/* actions when thread is to become active */
	int (*thread_activate) ( kthread_t * );

	/* actions when thread stopped to be active */
	int (*thread_deactivate) ( kthread_t * );

	/* set scheduler specific parameters */
	int (*set_sched_parameters) ( int sched_policy, sched_t *);

	/* get scheduler specific parameters */
	int (*get_sched_parameters) ( int sched_policy, sched_t *);

	/* set scheduler specific parameters to thread */
	int (*set_thread_sched_parameters) ( kthread_t *, sched_t *);

	/* get scheduler specific parameters from thread */
	int (*get_thread_sched_parameters) ( kthread_t *, sched_t *);

	ksched_params_t params;	/* scheduler specific data */
};

void ksched_init ();
ksched_t *ksched_get ( int sched_policy );

/*! Interface to threads ---------------------------------------------------- */

int sys__set_sched_params ( void *p );
int sys__get_sched_params ( void *p );
int sys__set_thread_sched_params ( void *p );
int sys__get_thread_sched_params ( void *p );

#endif
