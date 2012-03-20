/*! Monitor (synchronization mechanism) */
#define _KERNEL_

#define _MONITOR_C_
#include "monitor.h"

#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/kprint.h>
#include <kernel/errno.h>
#include <lib/types.h>

/*! Initialize new monitor */
int sys__monitor_init ( void *p )
{
	/* parameters on thread stack */
	monitor_t *monitor;
	/* local variables */
	kmonitor_t *kmonitor;

	monitor = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( monitor, E_INVALID_HANDLE );

	kmonitor = kmalloc ( sizeof (kmonitor_t) );
	ASSERT_ERRNO_AND_EXIT ( kmonitor, E_NO_MEMORY );

	kmonitor->lock = FALSE;
	kmonitor->owner = NULL;
	kthreadq_init ( &kmonitor->queue );

	monitor->ptr = kmonitor;

	EXIT ( SUCCESS );
}

/*! Destroy monitor (and unblock all threads blocked on it) */
int sys__monitor_destroy ( void *p )
{
	/* parameters on thread stack */
	monitor_t *monitor;
	/* local variables */
	kmonitor_t *kmonitor;

	monitor = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( monitor && monitor->ptr, E_INVALID_HANDLE );

	kmonitor = monitor->ptr;

	if ( kthreadq_release_all ( &kmonitor->queue ) )
		kthreads_schedule ();

	kfree ( kmonitor );
	monitor->ptr = NULL;

	EXIT ( SUCCESS );
}

/*! Initialize new monitor */
int sys__monitor_queue_init ( void *p )
{
	/* parameters on thread stack */
	monitor_q *queue;
	/* local variables */
	kmonitor_q *kqueue;

	queue = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( queue, E_INVALID_HANDLE );

	kqueue = kmalloc ( sizeof (kmonitor_q) );
	ASSERT_ERRNO_AND_EXIT ( kqueue, E_NO_MEMORY );

	kthreadq_init ( &kqueue->queue );

	queue->ptr = kqueue;

	EXIT ( SUCCESS );
}

/*! Destroy monitor (and unblock all threads blocked on it) */
int sys__monitor_queue_destroy ( void *p )
{
	/* parameters on thread stack */
	monitor_q *queue;
	/* local variables */
	kmonitor_q *kqueue;

	queue = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( queue && queue->ptr, E_INVALID_HANDLE );

	kqueue = queue->ptr;

	if ( kthreadq_release_all ( &kqueue->queue ) )
		kthreads_schedule ();

	kfree ( kqueue );
	queue->ptr = NULL;

	EXIT ( SUCCESS );
}

/*! Lock monitor (or block trying) */
int sys__monitor_lock ( void *p )
{
	/* parameters on thread stack */
	monitor_t *monitor;
	/* local variables */
	kmonitor_t *kmonitor;

	monitor = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( monitor && monitor->ptr, E_INVALID_HANDLE );

	kmonitor = monitor->ptr;

	SET_ERRNO ( SUCCESS );

	if ( !kmonitor->lock )
	{
		kmonitor->lock = TRUE;
		kmonitor->owner = kthread_get_active ();
	}
	else {
		kthread_enqueue ( NULL, &kmonitor->queue );
		kthreads_schedule ();
	}

	RETURN ( SUCCESS );
}

/*! Unlock monitor */
int sys__monitor_unlock ( void *p )
{
	/* parameters on thread stack */
	monitor_t *monitor;
	/* local variables */
	kmonitor_t *kmonitor;

	monitor = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( monitor && monitor->ptr, E_INVALID_HANDLE );

	kmonitor = monitor->ptr;

	ASSERT_ERRNO_AND_EXIT ( kmonitor->owner == kthread_get_active (),
				E_NOT_OWNER );

	SET_ERRNO ( SUCCESS );

	kmonitor->owner = kthreadq_get ( &kmonitor->queue );
	if ( !kthreadq_release ( &kmonitor->queue ) )
		kmonitor->lock = FALSE;
	else
		kthreads_schedule ();

	RETURN ( SUCCESS );
}

/*! Block thread (on conditional variable) and release monitor */
int sys__monitor_wait ( void *p )
{
	/* parameters on thread stack */
	monitor_t *monitor;
	monitor_q *queue;
	/* local variables */
	kmonitor_t *kmonitor;
	kmonitor_q *kqueue;

	monitor = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( monitor && monitor->ptr, E_INVALID_HANDLE );

	p += sizeof (void *);

	queue = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( queue && queue->ptr, E_INVALID_HANDLE );

	kmonitor = monitor->ptr;
	kqueue = queue->ptr;

	ASSERT_ERRNO_AND_EXIT ( kmonitor->owner == kthread_get_active (),
				E_NOT_OWNER );

	SET_ERRNO ( SUCCESS );

	kthread_set_qdata ( NULL, kmonitor );
	kthread_enqueue ( NULL, &kqueue->queue );

	kmonitor->owner = kthreadq_get ( &kmonitor->queue );
	if ( !kthreadq_release ( &kmonitor->queue ) )
		kmonitor->lock = FALSE;

	kthreads_schedule ();

	RETURN ( SUCCESS );
}

/* 'signal' and 'broadcast' are very similar - implemented in single function */
static int k_monitor_release ( void *p, int broadcast );

/*! Unblock thread from monitor queue */
int sys__monitor_signal ( void *p )
{
	return k_monitor_release ( p, FALSE );
}

/*! Unblock all threads from monitor queue */
int sys__monitor_broadcast ( void *p )
{
	return k_monitor_release ( p, TRUE );
}

/*! Release first or all threads from monitor queue (cond.var.) */
static int k_monitor_release ( void *p, int broadcast )
{
	/* parameters on thread stack */
	monitor_q *queue;
	/* local variables */
	kmonitor_q *kqueue;
	kthread_t *kthr;
	kmonitor_t *kmonitor;
	int reschedule = 0;

	queue = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	ASSERT_ERRNO_AND_EXIT ( queue && queue->ptr, E_INVALID_HANDLE );

	kqueue = queue->ptr;

	do {
		kthr = kthreadq_get ( &kqueue->queue ); /* first from queue */

		if ( !kthr )
			break;

		kmonitor = kthread_get_qdata ( kthr );

		if ( !kmonitor->lock ) /* monitor not locked? */
		{
			/* unblocked thread becomes monitor owner */
			kmonitor->lock = TRUE;
			kmonitor->owner = kthr;
			kthreadq_release ( &kqueue->queue );/*to ready threads*/
			reschedule++;
		}
		else {
			/* move thread from monitor queue (cond.var.)
			   to monitor entrance queue */
			kthr = kthreadq_remove ( &kqueue->queue, NULL );
			kthread_enqueue ( kthr, &kmonitor->queue );
		}
	}
	while ( kthr && broadcast );

	SET_ERRNO ( SUCCESS );

	if ( reschedule )
		kthreads_schedule ();

	RETURN ( SUCCESS );
}
