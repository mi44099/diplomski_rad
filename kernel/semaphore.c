/*! Simple semaphore */
#define _KERNEL_

#define _SEMAPHORE_C_
#include "semaphore.h"

#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/kprint.h>
#include <kernel/errno.h>
#include <lib/types.h>

/*! Initialize new semaphore with initial value */
int sys__sem_init ( void *p )
{
	sem_t *sem;
	int initial_value;
	ksem_t *ksem;

	sem = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	p += sizeof (void *);
	initial_value = *( (int *) p );

	ASSERT_ERRNO_AND_EXIT ( sem, E_INVALID_HANDLE );

	ksem = kmalloc ( sizeof (ksem_t) );
	ASSERT ( ksem );

	ksem->sem_value = initial_value;
	kthreadq_init ( &ksem->queue );

	sem->ptr = ksem;

	EXIT ( SUCCESS );
}

/*! Destroy semaphore (and unblock all threads blocked on it) */
int sys__sem_destroy ( void *p )
{
	ksem_t *ksem;
	sem_t *sem;

	sem = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );

	ASSERT_ERRNO_AND_EXIT ( sem && sem->ptr, E_INVALID_HANDLE );

	ksem = sem->ptr;

	if ( kthreadq_release_all ( &ksem->queue ) )
		kthreads_schedule ();

	kfree ( ksem );
	sem->ptr = NULL;

	EXIT ( SUCCESS );
}

/*! Increment semaphore value by 1 or unblock single blocked thread on it */
int sys__sem_post ( void *p )
{
	ksem_t *ksem;
	sem_t *sem;

	sem = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );

	ASSERT_ERRNO_AND_EXIT ( sem && sem->ptr, E_INVALID_HANDLE );

	ksem = sem->ptr;

	SET_ERRNO ( SUCCESS );

	if ( !kthreadq_release ( &ksem->queue ) )
		ksem->sem_value++;
	else
		kthreads_schedule ();

	RETURN ( SUCCESS );
}

/*! Decrement semaphore value by 1 or block calling thread if value == 0 */
int sys__sem_wait ( void *p )
{
	ksem_t *ksem;
	sem_t *sem;

	sem = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );

	ASSERT_ERRNO_AND_EXIT ( sem && sem->ptr, E_INVALID_HANDLE );

	ksem = sem->ptr;

	SET_ERRNO ( SUCCESS );

	if ( ksem->sem_value > 0 )
	{
		ksem->sem_value--;
	}
	else {
		kthread_enqueue ( NULL, &ksem->queue );
		kthreads_schedule ();
	}

	RETURN ( SUCCESS );
}
