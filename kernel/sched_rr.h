/*! Round Robin scheduler */

#pragma once

#ifdef _KERNEL_

#include <lib/types.h>

/*! Per thread scheduler data */
typedef struct _ksched_rr_thread_params_
{
	time_t slice_start;
	time_t slice_end;
	time_t remainder;
}
ksched_rr_thread_params;

/*! Round Robin global parameters */
typedef struct _ksched_rr_t_
{
	time_t time_slice;	/* time slice each thread is given at start */
	time_t threshold;	/* if remaining time is less than threshold
				   do not return to that thread, but schedule
				   next one */
	void *rr_alarm;		/* kernel alarm reference used in RR */
	alarm_t alarm;		/* alarm parameters */
}
ksched_rr_t;

#endif /* _KERNEL_ */