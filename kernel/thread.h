/*! Thread management */

#pragma once

/*! interface for threads (via software interrupt) -------------------------- */
int sys__create_thread ( void *p );
int sys__thread_exit ( void *p );
int sys__wait_for_thread ( void *p );
int sys__cancel_thread ( void *p );
int sys__thread_self ( void *p );

int sys__start_program ( void *p );

int sys__set_errno ( void *p );
int sys__get_errno ( void *p );

#ifdef _KERNEL_ /* for use inside 'kernel/ *' files */

#include <lib/types.h>
#include <lib/list.h>

/*! Thread queue */
typedef struct _kthread_q_
{
	list_t q;		/* queue implementation in list.h/list.c */
	/* uint flags; */	/* various flags, e.g. sort order */
}
kthread_q;

#ifndef _K_THREAD_C_	/* only 'kernel/thread.c' 'knows' thread descriptor */
typedef void *kthread_t;
#else /* only for 'kernel/thread.c' */
struct _kthread_t_;
typedef struct _kthread_t_ kthread_t;
#endif /* _K_THREAD_C_ */

#include <kernel/memory.h>
#include <kernel/messages.h>
#include <kernel/sched.h>

/*! Interface for kernel ---------------------------------------------------- */
void kthreads_init ();
kthread_t *kthread_start_process ( char *prog_name, void *param, int prio );
kthread_t *kthread_create ( void *start_func, void *param, void *exit_func,
			    int sched, int prio, void *stack, size_t stack_size,
			    int run, kprocess_t *proc );

/*! Interface to secondary schedulers */
void kthreads_schedule ();
void kthread_move_to_ready ( kthread_t *kthr, int where );
kthread_t *kthread_remove_from_ready ( kthread_t *kthr );
int kthread_cancel ( kthread_t *kthread, int exit_status );

/*! Get-ers and Set-ers */
extern inline int kthread_is_active ( kthread_t *kthread );
extern inline void *kthread_get_active ();
extern inline void *kthread_get_context ( kthread_t *thread );
extern inline int kthread_get_prio ( kthread_t *kthread );
int kthread_set_prio ( kthread_t *kthread, int prio );
extern inline kprocess_t *kthread_get_process ( kthread_t *kthread );
extern inline int kthread_get_id ( kthread_t *kthread );

extern inline int kthread_is_ready ( kthread_t *kthread );

extern inline void *kthread_create_private_storage (kthread_t *kthr, size_t s);
extern inline void kthread_delete_private_storage ( kthread_t *kthr, void *ps );
extern inline void kthread_set_private_storage ( kthread_t *kthr, void *ps );
extern inline void *kthread_get_private_storage ( kthread_t *kthr );

extern inline kthread_sched_data_t *kthread_get_sched_param ( kthread_t *kthr );


#ifdef	MESSAGES
extern inline kthrmsg_qs *kthread_get_msgqs ( kthread_t *thread );
#endif

extern inline kthread_t *kthread_get_descriptor ( thread_t *thr );

/*! Thread queue manipulation */
extern inline void kthreadq_init ( kthread_q *q );
extern inline void kthreadq_append ( kthread_q *q, kthread_t *kthr );
extern inline void kthreadq_prepend ( kthread_q *q, kthread_t *kthread );
extern inline kthread_t *kthreadq_remove ( kthread_q *q, kthread_t *kthr );
extern inline kthread_t *kthreadq_get ( kthread_q *q );
extern inline kthread_t *kthreadq_get_next ( kthread_t *kthr );

void kthread_enqueue ( kthread_t *kthr, kthread_q *q_id );
int kthreadq_release ( kthread_q *q_id );
int kthreadq_release_all ( kthread_q *q_id );

extern inline void kthread_set_qdata ( kthread_t *kthr, void *qdata );
extern inline void *kthread_get_qdata ( kthread_t *kthr );

/*! errno */
extern inline void kthread_set_errno ( kthread_t *kthr, int error_number );
extern inline int kthread_get_errno ( kthread_t *kthr );
extern inline void kthread_set_syscall_retval ( kthread_t *kthread, int ret_val );

int kthread_info ();


#ifdef _K_THREAD_C_ /* rest of the file is only for kernel/thread.c */

#include <arch/context.h>

/*! Thread descriptor */
struct _kthread_t_
{
	context_t context;	/* storage for thread context */

	uint id;		/* thread id */

	int state;		/* thread state */

	list_h ql;		/* list element for "thread state" list */

	int prio;		/* priority - primary scheduling parameter */

	kthread_sched_data_t sched;	/* secondary scheduler parameters */

	kthread_q *queue;	/* in witch queue (if not active) */
	void *qdata;		/* temporary storage for data while waiting */

	kthread_q join_queue;	/* queue for threads waiting for this to end */

	void *stack;		/* stack address and size (for deallocation) */
	uint stack_size;

	kprocess_t *proc;	/* to which process this thread belongs */

	void *private_storage;	/* thread private storage */

#ifdef	MESSAGES
	kthrmsg_qs msg;
#endif

	list_h all;		/* list element for list of all threads */

	int exit_status;	/* status with which thread exited */

	int errno;		/* exit status of last function call */

	int ref_cnt;		/* can we free this descriptor? */
};

/*! Thread states */
enum {
	THR_STATE_ACTIVE = 1,
	THR_STATE_READY,
	THR_STATE_WAIT,
	THR_STATE_PASSIVE
};

/* "ready" queue manipulation */
static void kthread_ready_list_init ();
static int kthread_ready_list_highest ();
static void kthread_ready_list_set_not_empty ( int index );
static void kthread_ready_list_set_empty ( int index );

static void kthread_remove_descriptor ( kthread_t *kthr );

/* idle thread */
static void idle_thread ( void *param );

#endif	/* _K_THREAD_C_ */
#endif	/* _KERNEL_ */
