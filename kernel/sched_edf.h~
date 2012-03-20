/*! EDF scheduler */

#pragma once

#ifdef _KERNEL_

#include <lib/types.h>
#include <kernel/thread.h>

/*! Per thread scheduler data */
typedef struct _ksched_edf_thread_params_t
{
	time_t deadline;
	time_t period;
	time_t next_run;
	int flags;

	void *edf_alarm;		/* kernel alarm reference used in EDF */
}
ksched_edf_thread_params_t;

/*! edf global parameters */
typedef struct _ksched_edf_t_
{
	kthread_t *active; 	//kazaljka na dretvu odabranu od EDFa

	kthread_q ready; 	//edf pripravne
	kthread_q wait; 	//edf pripravne
}
ksched_edf_t;

#endif /* _KERNEL_ */
