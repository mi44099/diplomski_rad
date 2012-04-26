/*! System call - call to kernel from threads (via software interrupt) */

#define _KERNEL_

#include "syscall.h"
#include <arch/syscall.h>
#include <arch/interrupts.h>
#include <arch/processor.h>

#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/time.h>
#include <kernel/semaphore.h>
#include <kernel/monitor.h>
#include <kernel/devices.h>
#include <kernel/memory.h>
#include <kernel/messages.h>

#include <kernel/errno.h>

/*! syscall handlers */
static int (*k_sysfunc[SYSFUNCS]) ( void *params ) =
{
	NULL,

	sys__create_thread,
	sys__thread_exit,
	sys__wait_for_thread,
	sys__cancel_thread,
	sys__thread_self,
	sys__start_program,

	sys__set_sched_params,
	sys__get_sched_params,
	sys__set_thread_sched_params,
	sys__get_thread_sched_params,

	sys__set_errno,
	sys__get_errno,

	sys__get_time,
	sys__alarm_new,
	sys__alarm_set,
	sys__alarm_get,
	sys__wait_for_alarm,
	sys__alarm_remove,

	sys__sem_init,
	sys__sem_destroy,
	sys__sem_post,
	sys__sem_wait,

	sys__monitor_init,
	sys__monitor_destroy,
	sys__monitor_queue_init,
	sys__monitor_queue_destroy,
	sys__monitor_lock,
	sys__monitor_unlock,
	sys__monitor_wait,
	sys__monitor_signal,
	sys__monitor_broadcast,

	sys__device_send,
	sys__device_recv,
	sys__device_open,
	sys__device_close,
	sys__device_lock,
	sys__device_unlock,
	sys__set_default_stdin,
	sys__set_default_stdout,

	sys__thread_msg_set,
	sys__create_msg_queue,
	sys__delete_msg_queue,
	sys__msg_post,
	sys__msg_recv,

	sys__sysinfo,

	sys__suspend
};

/*!
 * Process syscalls
 * (syscall is forwarded from arch interrupt subsystem to k_syscall)
 */
void k_syscall ( uint irqn )
{
	int id, retval;
	void *context, *params;

	ASSERT ( irqn == SOFTWARE_INTERRUPT );

	context = kthread_get_context ( NULL ); //active thread context

	id = arch_syscall_get_id ( context );

	ASSERT ( id >= 0 && id < SYSFUNCS );

	params = arch_syscall_get_params ( context );

	retval = k_sysfunc[id] ( params );

	if ( id != THREAD_EXIT )
		arch_syscall_set_retval ( context, retval );
}

/*! Stop processor until next interrupt occurs - for idle thread only! */
int sys__suspend ( void *p )
{
	enable_interrupts ();
	suspend ();

	return 0;
}
