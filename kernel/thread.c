/*! Thread management */
#define _KERNEL_

#define _K_THREAD_C_
#include "thread.h"

#include <arch/interrupts.h>
#include <arch/syscall.h>
#include <kernel/memory.h>
#include <kernel/devices.h>
#include <kernel/kprint.h>
#include <kernel/errno.h>
#include <kernel/sched.h>
#include <lib/bits.h>
#include <lib/list.h>
#include <lib/string.h>
#include <arch/processor.h>

#ifdef	MESSAGES
#include <kernel/messages.h>
#endif

static list_t all_threads; /* all threads */

static kthread_t *active_thread = NULL; /* active thread */

static kthread_q ready_q[PRIO_LEVELS]; /* ready threads organized by priority */

kprocess_t kernel_proc; /* kernel process (currently only for idle thread) */
static list_t procs; /* list of all processes */

/*! initialize thread structures and create idle thread */
void kthreads_init ()
{
	list_init ( &all_threads );
	list_init ( &procs );

	/* queue for ready threads is empty */
	kthread_ready_list_init ();

	ksched_init ();

	active_thread = NULL;

	/* initially create 'idle thread' */
	kernel_proc.prog = NULL;
	kernel_proc.stack_pool = NULL;
	kernel_proc.m.start = NULL;
	kernel_proc.m.size = (size_t) 0xffffffff;

	(void) kthread_create ( idle_thread, NULL, NULL, 0, 0, NULL, 0, 1,
				&kernel_proc );

	kthreads_schedule ();
}

/*!
 * Start program defined by 'prog_name' (loaded as module) as new process:
 * - initialize environment (stack area for threads, stdin, stdout) and start
 *   it's first thread
 * \param prog_name Program name (as given with module)
 * \param param Command line arguments for starting thread (if not NULL)
 * \param prio Priority for starting thread
 * \return Pointer to descriptor of created process
 */
kthread_t *kthread_start_process ( char *prog_name, void *param, int prio )
{
	extern kdevice_t *u_stdin, *u_stdout;
	extern list_t progs;
	kprog_t *prog;
	kprocess_t *proc;
	kthread_t *kthread;
	char **args = NULL, *arg, *karg, **kargs;
	size_t argsize;
	int i;

	prog = list_get ( &progs, FIRST );
	while ( prog && strcmp ( prog->prog_name, prog_name ) )
		prog = list_get_next ( &prog->all );

	if ( !prog )
		return NULL;

	/* create new process */
	proc = kmalloc ( sizeof ( kprocess_t) );
	ASSERT ( proc );

	proc->prog = prog;
	proc->m.size = prog->m.size + prog->pi->heap_size + prog->pi->stack_size;

	proc->m.start = proc->pi = kmalloc ( proc->m.size );

	if ( !proc->pi )
	{
		kprint ( "Not enough memory! (%d)\n", proc->m.size );
		kfree ( proc->pi );
		return NULL;
	}

	/* copy code and data */
	memcpy ( proc->pi, prog->pi, prog->m.size );

	/* define heap and stack */
	proc->pi->heap = (void *) proc->pi + prog->m.size;
	proc->pi->stack = proc->pi->heap + prog->pi->heap_size;
	memset (proc->pi->heap, 0, prog->pi->heap_size + prog->pi->stack_size);
	proc->m.start = proc->pi;

	proc->pi->stdin = u_stdin;
	proc->pi->stdout = u_stdout;

	/* initialize memory pool for threads stacks */
	proc->stack_pool = ffs_init ( proc->pi->stack, prog->pi->stack_size );

	/* set addresses in process header to relative addresses */
	proc->pi->heap = (void *) prog->m.size;
	proc->pi->stack = proc->pi->heap + prog->pi->heap_size;
	proc->pi->end_adr = proc->pi->stack + prog->pi->stack_size;

	proc->thr_count = 0;

	if ( !prio )
		prio = proc->pi->prio;
	if ( !prio )
		prio = THR_DEFAULT_PRIO;

	if ( param ) /* have arguments? */
	{
		/* copy command line arguments from kernel space to process;
		   (use process stack space for arguments) */
		kargs = param;
		for ( i = 0; kargs[i]; i++ ) ;
		argsize = ( (size_t) kargs[i-1] + strlen( kargs[i-1] ) + 1 )
			  - (size_t) param;
		if ( argsize > 0 )
		{
			args = ffs_alloc ( proc->stack_pool, argsize );
			arg = (void *) args + (i + 1) * sizeof (void *);
			kargs = param;
			i = 0;
			do {
				karg = kargs[i];
				strcpy ( arg, karg );
				args[i++] = K2U_GET_ADR ( arg, proc );
				arg += strlen ( arg ) + 1;
			}
			while ( kargs[i] );
			args[i] = NULL;
			args = K2U_GET_ADR ( args, proc );
		}

		kfree ( param );
	}
	kthread = kthread_create ( proc->pi->init, args, NULL, 0, prio,
				   NULL, 0, 1, proc );

	list_append ( &procs, proc, &proc->all );

	kthreads_schedule ();

	return kthread;
}

/*!
 * Create new thread
 * \param start_func Starting function for new thread
 * \param param Parameter sent to starting function
 * \param exit_func Thread will call this function when it leaves 'start_func'
 * \param prio Thread priority
 * \param stack Address of thread stack (if not NULL)
 * \param stack_size Stack size
 * \param run Move thread descriptor to ready threads?
 * \param proc Process descriptor thread belongs to
 * \return Pointer to descriptor of created kernel thread
 */
kthread_t *kthread_create ( void *start_func, void *param, void *exit_func,
			    int sched_policy, int prio, void *stack,
			    size_t stack_size, int run, kprocess_t *proc )
{
	kthread_t *kthread;

	/* if stack is not defined */
	if ( proc && proc->stack_pool && ( !stack || !stack_size ) )
	{
		stack_size = proc->pi->thread_stack;
		stack = ffs_alloc ( proc->stack_pool, stack_size );
	}
	else if ( !stack || !stack_size )
	{
		if ( !stack_size )
			stack_size = DEFAULT_THREAD_STACK_SIZE;

		stack = kmalloc ( stack_size );
	}
	ASSERT ( stack && stack_size );

	kthread = kmalloc ( sizeof (kthread_t) ); /* thread descriptor */
	ASSERT ( kthread );

	/* initialize thread descriptor */
	kthread->id = k_new_unique_id ();

	kthread->state = THR_STATE_PASSIVE;

	if ( prio < 0 ) prio = 0;
	if ( prio >= PRIO_LEVELS ) prio = PRIO_LEVELS - 1;
	kthread->prio = prio;


	arch_create_thread_context ( &kthread->context, start_func, param,
				     exit_func, stack, stack_size, proc );
	kthread->queue = NULL;
	kthread->exit_status = 0;
	kthreadq_init ( &kthread->join_queue );
	kthread->ref_cnt = 0;

	if ( run ) {
		kthread_move_to_ready ( kthread, LAST );
		kthread->ref_cnt = 1;
	}

	kthread->stack = stack;
	kthread->stack_size = stack_size;
	kthread->proc = proc;
	kthread->proc->thr_count++;
	kthread->private_storage = NULL;

#ifdef	MESSAGES
	k_thr_msg_init ( &kthread->msg );
#endif
	list_append ( &all_threads, kthread, &kthread->all );

	ksched_thread_add ( kthread, sched_policy );

	return kthread;
}

/*!
 * Select ready thread with highest priority  as active
 * - if different from current, move current into ready queue (id not NULL) and
 *   move selected thread from ready queue to active queue
 */
void kthreads_schedule ()
{
	int highest_prio;
	kthread_t *curr, *next;

//LOG ( DEBUG, "%x [active 1 THR_SCHEDULE]", active_thread );

	curr = active_thread;

	highest_prio = kthread_ready_list_highest ();

	/* must exist an thread to return to, 'curr' or first from 'ready' */
	ASSERT ( ( curr && curr->state == THR_STATE_ACTIVE ) ||
		 highest_prio >= 0 );

	if ( !curr || curr->state != THR_STATE_ACTIVE ||
	     highest_prio > curr->prio )
	{
		if ( curr ) /* change active thread */
		{
			ksched_deactivate_thread ( curr );

			/* move last active to ready queue, if still ready */
			if ( curr->state == THR_STATE_ACTIVE )
				kthread_move_to_ready ( curr, LAST );

			/* deactivation might change ready thread list */
			highest_prio = kthread_ready_list_highest ();
		}

		next = kthreadq_remove ( &ready_q[highest_prio], NULL );
		ASSERT ( next );

		/* no more ready threads in list? */
		if ( kthreadq_get ( &ready_q[highest_prio] ) == NULL )
			kthread_ready_list_set_empty ( highest_prio );

		active_thread = next;
		active_thread->state = THR_STATE_ACTIVE;
		active_thread->queue = NULL;
		ksched_activate_thread ( active_thread );
	}

//LOG ( DEBUG, "%x [active 2 THR_SCHEDULE]", active_thread );
	/* select 'active_thread' context */
	arch_select_thread ( &active_thread->context );
}

/*! operations on thread queues (blocked threads) --------------------------- */

/*!
 * Put given thread or active thread (when kthread == NULL) into queue 'q_id'
 * - if kthread != NULL, thread must not be in any list and 'kthreads_schedule'
 *   should follow this call before exiting from kernel!
 */
void kthread_enqueue ( kthread_t *kthread, kthread_q *q )
{
	ASSERT ( ( kthread || active_thread ) && q );

	if ( !kthread )
		kthread = active_thread;

	kthread->state = THR_STATE_WAIT;
	kthread->queue = q;

	kthreadq_append ( kthread->queue, kthread );
}

/*!
 * Release single thread from given queue (if queue not empty)
 * \param q Queue
 * \return 1 if thread was released, 0 if queue was empty
 */
int kthreadq_release ( kthread_q *q )
{
	kthread_t *kthread;

	ASSERT ( q );

	kthread = kthreadq_remove ( q, NULL );

	if ( kthread )
	{
		kthread_move_to_ready ( kthread, LAST );
		return 1;
	}
	else {
		return 0;
	}
}

/*!
 * Release all threads from given queue (if queue not empty)
 * \param q Queue
 * \return number of thread released, 0 if queue was empty
 */
int kthreadq_release_all ( kthread_q *q )
{
	int cnt = 0;

	while ( kthreadq_release (q) )
		cnt++;

	return cnt;
}

/*! Ready thread list (multi-level organized; one level per priority) ------- */

/* masks for fast searching for highest priority ready thread */
#define RDY_MASKS ( ( PRIO_LEVELS + sizeof (word_t) - 1 ) / sizeof (word_t) )
static word_t rdy_mask[ RDY_MASKS ];

/*! Initialize ready thread list */
static void kthread_ready_list_init ()
{
	int i;

	/* queue for ready threads is empty */
	for ( i = 0; i < PRIO_LEVELS; i++ )
		kthreadq_init ( &ready_q[i] );

	for ( i = 0; i < RDY_MASKS; i++ )
		rdy_mask[i] = 0;

}

/*! Find and return priority of highest priority thread in ready list */
static int kthread_ready_list_highest ()
{
	int i, first = -1;

	for ( i = RDY_MASKS - 1; i >= 0; i-- )
		if ( rdy_mask[i] )
		{
			first = i * sizeof (word_t) + msb_index ( rdy_mask[i] );
			break;
		}

	return first;
}

/*! Mark ready list for given priority (level) as non-empty */
static void kthread_ready_list_set_not_empty ( int index )
{
	int i, j;

	i = index / sizeof (word_t);
	j = index % sizeof (word_t);

	rdy_mask[i] |= (word_t) ( 1 << j );
}

/*! Mark ready list for given priority (level) as empty */
static void kthread_ready_list_set_empty ( int index )
{
	int i, j;

	i = index / sizeof (word_t);
	j = index % sizeof (word_t);

	rdy_mask[i] &= ~( (word_t) ( 1 << j ) );
}

/*!
 * Move given thread (its descriptor) to ready threads
 * (as last or first in its priority queue)
 */
void kthread_move_to_ready ( kthread_t *kthread, int where )
{
	kthread->state = THR_STATE_READY;
	kthread->queue = &ready_q[kthread->prio];

	if ( where == LAST )
		kthreadq_append ( kthread->queue, kthread );
	else
		kthreadq_prepend ( kthread->queue, kthread );

	kthread_ready_list_set_not_empty ( kthread->prio );
}

/*! Remove given thread (its descriptor) from ready threads */
kthread_t *kthread_remove_from_ready ( kthread_t *kthread )
{
	if ( !kthread )
		return NULL;

	kthread->queue = &ready_q[kthread->prio];

	if ( kthreadq_remove ( kthread->queue, kthread ) != kthread )
		return NULL;

	/* no more ready threads in list? */
	if ( kthreadq_get ( kthread->queue ) == NULL )
		kthread_ready_list_set_empty ( kthread->prio );

	return kthread;
}

/*! Internal function for removing (freeing) thread descriptor */
static void kthread_remove_descriptor ( kthread_t *kthread )
{
	kthread_t *test;

	k_free_unique_id ( kthread->id );
	kthread->id = 0;

#ifdef DEBUG
	test = list_find_and_remove ( &all_threads, &kthread->all );
	ASSERT ( test == kthread );
#else
	(void) list_remove ( &all_threads, 0, &kthread->all );
#endif

	kfree ( kthread );
}

/*!
 * Cancel thread
 * \param kthread Thread descriptor
 */
int kthread_cancel ( kthread_t *kthread, int exit_status )
{
	void *test;

	if ( kthread->state == THR_STATE_PASSIVE )
		return SUCCESS; /* thread is already finished */

	if ( kthread->state == THR_STATE_READY )
	{
		/* remove target 'thread' from its queue */
		kthreadq_remove ( kthread->queue, kthread );
		if ( kthreadq_get ( &ready_q[kthread->prio] ) == NULL )
			kthread_ready_list_set_empty ( kthread->prio );
	}
	else if ( kthread->state == THR_STATE_WAIT )
	{
		/* remove target 'thread' from its queue */
		kthreadq_remove ( kthread->queue, kthread );
	}
	else if ( kthread->state == THR_STATE_ACTIVE )
	{
		if ( kthread != active_thread )
			return E_DONT_EXIST; /* thread descriptor corrupted ! */

		active_thread->state = THR_STATE_PASSIVE;
	}
	else {
		return E_INVALID_HANDLE; /* thread descriptor corrupted ! */
	}

	kthread->ref_cnt--;
	kthread->exit_status = exit_status;
	kthread->proc->thr_count--;


#ifdef	MESSAGES
	k_msgq_clean ( &kthread->msg.msgq );
#endif

	/* remove it from its scheduler */
	ksched_thread_remove ( kthread );

	/* release thread stack */
	if ( kthread->stack )
	{
		if ( kthread->proc->m.start ) /* user level thread */
			ffs_free ( kthread->proc->stack_pool,
				   kthread->stack );
		else /* kernel level thread */
			kfree ( kthread->stack );
	}

	kthread_delete_private_storage ( kthread, kthread->private_storage );

	if ( kthread->proc->thr_count == 0 && kthread->proc->pi )
	{
		/* last (non-kernel) thread - remove process */
		kfree ( kthread->proc->pi );
#ifdef DEBUG
		test = list_find_and_remove ( &procs, &kthread->proc->all );
		ASSERT ( test == kthread->proc );
#else
		(void) list_remove ( &procs, 0, &kthread->proc->all );
#endif
		kfree ( kthread->proc );
	}

	if ( !kthread->ref_cnt )
	{
		kthread_remove_descriptor ( kthread );

		kthread = NULL;
	}
	else {
		kthreadq_release_all ( &kthread->join_queue );
	}

	kthreads_schedule ();

	return SUCCESS;
}



/*! Idle thread ------------------------------------------------------------- */
#include <api/syscall.h>

/*! Idle thread starting (and only) function */
static void idle_thread ( void *param )
{
	while (1)
		syscall ( SUSPEND, NULL );
}


/*! >>> (Syscall) interface to threads -------------------------------------- */

/*!
 * Create new thread (params on user stack!)
 * \param func Starting function
 * \param param Parameter for starting function
 * \param prio Priority for new thread
 * \param thr_desc User level thread descriptor
 * (parameters are on calling thread stack)
 */
int sys__create_thread ( void *p )
{
	void *func;
	void *param;
	int sched, prio;
	thread_t *thr_desc;

	kthread_t *kthread;

	func = *( (void **) p ); p += sizeof (void *);

	param = *( (void **) p ); p += sizeof (void *);

	sched = *( (int *) p ); p += sizeof (int);

	prio = *( (int *) p ); p += sizeof (int);

	kthread = kthread_create (func, param, active_thread->proc->pi->exit,
				  sched, prio, NULL, 0, 1, active_thread->proc);

	ASSERT_ERRNO_AND_EXIT ( kthread, E_NO_MEMORY );

	thr_desc = *( (void **) p );
	if ( thr_desc )
	{
		thr_desc = U2K_GET_ADR ( thr_desc, active_thread->proc );
		thr_desc->thread = kthread;
		thr_desc->thr_id = kthread->id;
	}

	SET_ERRNO ( SUCCESS );

	kthreads_schedule ();

	RETURN ( SUCCESS );
}

/*!
 * End current thread (exit from it)
 * \param status Exit status number
 */
int sys__thread_exit ( void *p )
{
	int status;

//LOG(DEBUG, "%x THREAD_EXIT (EXIT)", active_thread );

	status = *( (int *) p );
	kthread_cancel ( active_thread, status );

//LOG(DEBUG, "%x THREAD_EXIT (SELECTED)", active_thread );

	return 0;
}

/*!
 * Wait for thread termination
 * \param thread Thread descriptor (user level descriptor)
 * \param wait Wait if thread not finished (!=0) or not (0)?
 * \return 0 if thread already gone; -1 if not finished and 'wait' not set;
 *         'thread exit status' otherwise
 */
int sys__wait_for_thread ( void *p )
{
	thread_t *thread;
	int wait;
	kthread_t *kthread;
	int ret_value = 0;

	thread = U2K_GET_ADR ( *( (void **) p ), active_thread->proc );
	p += sizeof (void *);

	wait = *( (int *) p );

	ASSERT_ERRNO_AND_EXIT ( thread && thread->thread, E_INVALID_HANDLE );

	kthread = thread->thread;

	if ( kthread->id != thread->thr_id ) /* at 'kthread' is now something else */
	{
		ret_value = -SUCCESS;
		SET_ERRNO ( SUCCESS );
	}
	else if ( kthread->state != THR_STATE_PASSIVE && !wait )
	{
		ret_value = -E_NOT_FINISHED;
		SET_ERRNO ( E_NOT_FINISHED );
	}
	else if ( kthread->state != THR_STATE_PASSIVE )
	{
		kthread->ref_cnt++;

		ret_value = -E_RETRY; /* retry (collect thread status) */
		SET_ERRNO ( E_RETRY );

		kthread_enqueue ( NULL, &kthread->join_queue );

		kthreads_schedule ();
	}
	else {
		/* kthread->state == THR_STATE_PASSIVE, but thread descriptor still
		   not freed - some thread still must collect its status */
		SET_ERRNO ( SUCCESS );
		ret_value = kthread->exit_status;

		kthread->ref_cnt--;

		if ( !kthread->ref_cnt )
			kthread_remove_descriptor ( kthread );
	}

	return ret_value;
}

/*!
 * Cancel some other thread
 * \param thread Thread descriptor (user)
 */
int sys__cancel_thread ( void *p )
{
	thread_t *thread;
	kthread_t *kthread;

	thread = U2K_GET_ADR ( *( (void **) p ), active_thread->proc );

	ASSERT_ERRNO_AND_EXIT ( thread && thread->thread, E_INVALID_HANDLE );

	kthread = thread->thread;

	if ( kthread->id != thread->thr_id )
		EXIT ( SUCCESS ); /* thread is already finished */

	/* remove thread from queue where its descriptor is */
	switch ( kthread->state )
	{
	case THR_STATE_PASSIVE:
		EXIT ( SUCCESS ); /* thread is already finished */

	case THR_STATE_READY:
	case THR_STATE_WAIT:
		EXIT ( kthread_cancel ( kthread, -1 ) );
		break;

	case THR_STATE_ACTIVE: /* can't cancel itself (or could!?) */
	default:
		EXIT ( E_INVALID_HANDLE ); /* thread descriptor corrupted ! */
	}
}

/*! Return calling thread descriptor
 * \param thread Thread descriptor (user level descriptor)
 * \return 0
 */
int sys__thread_self ( void *p )
{
	thread_t *thread;

	thread = U2K_GET_ADR ( *( (void **) p ), active_thread->proc );

	ASSERT_ERRNO_AND_EXIT ( thread, E_INVALID_HANDLE );

	thread->thread = active_thread;
	thread->thr_id = active_thread->id;

	EXIT ( SUCCESS );
}

/*!
 * Start new process
 * \param prog_name Program name (as given with module)
 * \param thr_desc Pointer to thread descriptor (user) for starting thread
 * \param param Command line arguments for starting thread (if not NULL)
 * \param prio Priority for starting thread
 */
int sys__start_program ( void *p )
{
	char *prog_name;
	void *param;
	int prio;
	thread_t *thr_desc;
	kthread_t *kthread, *cur = active_thread;
	char *arg, *karg, **args, **kargs = NULL;
	int argnum, argsize;

	prog_name = *( (void **) p ); p += sizeof (void *);
	ASSERT_ERRNO_AND_EXIT ( prog_name, E_INVALID_HANDLE );

	prog_name = U2K_GET_ADR ( prog_name, cur->proc );

	thr_desc = *( (void **) p ); p += sizeof (void *);

	param = *( (void **) p ); p += sizeof (void *);

	prio = *( (int *) p );

	if ( param ) /* copy parameters from one process space to another */
	{
		/* copy parameters to new process address space */
		/* first copy them to kernel */
		argnum = 0;
		argsize = 0;
		args = U2K_GET_ADR ( param, cur->proc );
		while ( args[argnum] )
		{
			arg = U2K_GET_ADR ( args[argnum++], cur->proc );
			argsize += strlen ( arg ) + 1;
		}
		if ( argnum > 0 )
		{
			kargs = kmalloc ( (argnum + 1) * sizeof (void *) +
					      argsize );
			karg = (void *) kargs + (argnum + 1) * sizeof (void *);
			argnum = 0;
			while ( args[argnum] )
			{
				arg = U2K_GET_ADR ( args[argnum], cur->proc );
				strcpy ( karg, arg );
				kargs[argnum++] = karg;
				karg += strlen ( karg ) + 1;
			}
			kargs[argnum] = NULL;
		}
	}

	SET_ERRNO ( SUCCESS );

	kthread = kthread_start_process ( prog_name, kargs, prio );

	if ( !kthread )
		EXIT ( E_NO_MEMORY );

	if ( thr_desc ) /* save thread descriptor */
	{
		thr_desc = U2K_GET_ADR ( thr_desc, cur->proc );
		thr_desc->thread = kthread;
		thr_desc->thr_id = kthread->id;
	}

	RETURN ( SUCCESS );
}

/*! Display info on threads */
int kthread_info ()
{
	kthread_t *kthread;
	int i = 1;

	kprint ( "Threads info\n" );

	kprint ( "[this]\tid=%d (desc. at %x) in process at %x, size=%d\n",
		  active_thread->id, active_thread, active_thread->proc->m.start,
		  active_thread->proc->m.size );

	kprint ( "\tprio=%d, state=%d, ret_val=%d\n",
		  active_thread->prio, active_thread->state,
		  active_thread->exit_status );

	kthread = list_get ( &all_threads, FIRST );
	while ( kthread )
	{
		kprint ( "[%d]\tid=%d (desc. at %x) in process at %x, size=%d\n",
			 i++, kthread->id, kthread, kthread->proc->m.start,
			 kthread->proc->m.size );

		kprint ( "\tprio=%d, state=%d, ret_val=%d\n",
			 kthread->prio, kthread->state,
			 kthread->exit_status );

		kthread = list_get_next ( &kthread->all );
	}

	return 0;
}

/*! Set and get current thread error status */
int sys__set_errno ( void *p )
{
	active_thread->errno = *( (int *) p );

	return 0;
}
int sys__get_errno ( void *p )
{
	return active_thread->errno;
}

/*! <<< Interface to threads ------------------------------------------------ */


/*! Get-ers, Set-ers and misc ----------------------------------------------- */

inline int kthread_is_active ( kthread_t *kthread )
{
	if ( kthread->state == THR_STATE_ACTIVE )
		return 1;
	else
		return 0;
}
inline void *kthread_get_active ()
{
	return (void *) active_thread;
}
inline void *kthread_get_context ( kthread_t *kthread )
{
	if ( kthread )
		return &kthread->context;
	else
		return &active_thread->context;
}
inline int kthread_get_prio ( kthread_t *kthread )
{
	if ( kthread )
		return kthread->prio;
	else
		return active_thread->prio;
}
int kthread_set_prio ( kthread_t *kthread, int prio )
{
	kthread_t *kthr = kthread;
	int old_prio;

	if ( !kthr )
		kthr = active_thread;

	old_prio = kthr->prio;

	/* change thread priority:
	(i)	if its active: change priority and move to ready
	(ii)	if its ready: remove from queue, change priority, put back
	(iii)	if its blocked: if queue is sorted by priority, same as (ii)
	*/
	switch ( kthr->state )
	{
	case THR_STATE_ACTIVE:
		kthr->prio = prio;
		kthread_move_to_ready ( kthr, LAST );
		kthreads_schedule ();
		break;

	case THR_STATE_READY:
		kthread_remove_from_ready (kthr);
		kthr->prio = prio;
		kthread_move_to_ready ( kthr, LAST );
		kthreads_schedule ();
		break;

	case THR_STATE_WAIT: /* as now there is only FIFO queue */
		kthr->prio = prio;
		break;

	case THR_STATE_PASSIVE: /* report error or just change priority? */
		kthr->prio = prio;
		break;
	}

	return old_prio;
}

inline kthread_sched_data_t *kthread_get_sched_param ( kthread_t *kthread )
{
	if ( kthread )
		return &kthread->sched;
	else
		return &active_thread->sched;
}

inline kprocess_t *kthread_get_process ( kthread_t *kthread )
{
	if ( kthread )
		return kthread->proc;
	else
		return active_thread->proc;
}

inline int kthread_get_id ( kthread_t *kthread )
{
	if ( kthread )
		return kthread->id;
	else
		return active_thread->id;
}

inline int kthread_is_ready ( kthread_t *kthread )
{
	kthread_t *kthr = kthread;

	if ( !kthr )
		kthr = active_thread;

	if ( kthr->state == THR_STATE_ACTIVE || kthr->state == THR_STATE_READY )
		return TRUE;
	else
		return FALSE;
}

/*! thread queue manipulation */
inline void kthreadq_init ( kthread_q *q )
{
	list_init ( &q->q );
}
inline void kthreadq_append ( kthread_q *q, kthread_t *kthread )
{
	list_append ( &q->q, kthread, &kthread->ql );
}
inline void kthreadq_prepend ( kthread_q *q, kthread_t *kthread )
{
	list_prepend ( &q->q, kthread, &kthread->ql );
}
inline kthread_t *kthreadq_remove ( kthread_q *q, kthread_t *kthread )
{
	if ( kthread )
		return list_find_and_remove ( &q->q, &kthread->ql );
	else
		return list_remove ( &q->q, FIRST, NULL );
}
inline kthread_t *kthreadq_get ( kthread_q *q )
{
	return list_get ( &q->q, FIRST );
}
inline kthread_t *kthreadq_get_next ( kthread_t *kthread )
{
	return list_get_next ( &kthread->ql );   /* kthread->queue->q.first->object */
}

/*! Temporary storage for blocked thread (save specific context before wait) */
inline void kthread_set_qdata ( kthread_t *kthread, void *qdata )
{
	if ( !kthread )
		kthread = active_thread;

	kthread->qdata = qdata;
}
inline void *kthread_get_qdata ( kthread_t *kthread )
{
	if ( !kthread )
		return active_thread->qdata;
	else
		return kthread->qdata;
}

/*! Get kernel thread descriptor from user thread descriptor */
inline kthread_t *kthread_get_descriptor ( thread_t *thr )
{
	kthread_t *kthread;

	if ( thr && (kthread = thr->thread) && thr->thr_id == kthread->id &&
		kthread->state != THR_STATE_PASSIVE )
		return kthread;
	else
		return NULL;
}

#ifdef	MESSAGES
inline kthrmsg_qs *kthread_get_msgqs ( kthread_t *thread )
{
	return &thread->msg;
}
#endif

/*! Thread private storage */
inline void *kthread_create_private_storage ( kthread_t *kthread, size_t s )
{
	return ffs_alloc ( kthread->proc->stack_pool, s );
}
inline void kthread_delete_private_storage ( kthread_t *kthread, void *ps )
{
	if ( ps )
		ffs_free ( kthread->proc->stack_pool, ps );
}
inline void kthread_set_private_storage ( kthread_t *kthread, void *ps )
{
	kthread->private_storage = ps;
}
inline void *kthread_get_private_storage ( kthread_t *kthread )
{
	return kthread->private_storage;
}

/*! errno */
inline void kthread_set_errno ( kthread_t *kthread, int error_number )
{
	if ( kthread )
		kthread->errno = error_number;
	else
		active_thread->errno = error_number;
}

inline int kthread_get_errno ( kthread_t *kthread )
{
	if ( kthread )
		return kthread->errno;
	else
		return active_thread->errno;
}

inline void kthread_set_syscall_retval ( kthread_t *kthread, int ret_val )
{
	arch_syscall_set_retval ( kthread_get_context(kthread), ret_val );
}
