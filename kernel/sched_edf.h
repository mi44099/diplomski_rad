/*! EDF scheduler */

#pragma once

#ifdef _KERNEL_

#include <lib/types.h>
#include <kernel/thread.h>

/*! Per thread scheduler data */
typedef struct _ksched_edf_thread_params_t
{
	time_t relative_deadline;
	time_t period;
	time_t next_run;
	time_t active_deadline;
	int flags;

	void *edf_alarm;		/* kernel alarm reference used in EDF */
}
ksched_edf_thread_params_t;

/*! edf global parameters */
typedef struct _ksched_edf_t_
{
	kthread_t *active; /* thread selected by EDF as top priority */

	kthread_q ready;
	kthread_q wait;
}
ksched_edf_t;

#endif /* _KERNEL_ */
