/*! Messages */
#ifdef MESSAGES

#define _KERNEL_

#define _K_MESSAGES_C_
#include "messages.h"

#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/kprint.h>
#include <kernel/errno.h>
#include <lib/list.h>
#include <lib/string.h>

/* list of all global message queues */
static list_t kmsg_qs = LIST_T_NULL;

/*! Initialize messaging part of new thread descriptor */
void k_thr_msg_init ( kthrmsg_qs *thrmsg )
{
	list_init ( &thrmsg->msgq.msgs );
	kthreadq_init ( &thrmsg->msgq.thrq );
	thrmsg->msgq.min_prio = 0;

	thrmsg->sig_prio = 0;
	thrmsg->signal_handler = NULL;
}

/*! Define thread behavior towards messages and signals */
int sys__thread_msg_set ( void *p )
{
	/* parameters on thread stack */
	uint min_msg_prio;
	int min_sig_prio;
	void *sig_handler;
	/* local variables */
	kthread_t *kthr;
	kthrmsg_qs *thrmsg;

	min_msg_prio = *( (uint *) p );		p += sizeof (uint);
	min_sig_prio = *( (int *) p );		p += sizeof (int);
	sig_handler = *( (void **) p );

	kthr = kthread_get_active ();
	thrmsg = kthread_get_msgqs ( kthr );

	thrmsg->msgq.min_prio = min_msg_prio;
	thrmsg->sig_prio = min_sig_prio;
	thrmsg->signal_handler = sig_handler;

	EXIT ( SUCCESS );
}

/*! Create global message queue */
int sys__create_msg_queue ( void *p )
{
	/* parameters on thread stack */
	msg_q *msgq;
	uint min_prio;
	/* local variables */
	kgmsg_q *gmsgq;

	msgq = *( (msg_q **) p );	p += sizeof (msg_q *);
	min_prio = *( (uint *) p );

	ASSERT_ERRNO_AND_EXIT ( msgq, E_INVALID_HANDLE );

	msgq = U2K_GET_ADR ( msgq, kthread_get_process (NULL) );

	gmsgq = kmalloc ( sizeof ( kgmsg_q ) );
	ASSERT_ERRNO_AND_EXIT ( gmsgq, E_NO_MEMORY );

	list_init ( &gmsgq->mq.msgs ); /* list for messages */
	kthreadq_init ( &gmsgq->mq.thrq ); /* list for blocked threads */

	gmsgq->mq.min_prio = min_prio;
	msgq->id = gmsgq->id = k_new_unique_id ();
	msgq->handle = gmsgq;

	list_append ( &kmsg_qs, gmsgq, &gmsgq->all ); /* all msg.q. list */

	EXIT ( SUCCESS );
}

/*! Delete global message queue */
int sys__delete_msg_queue ( void *p )
{
	/* parameter on thread stack */
	msg_q *msgq;
	/* local variables */
	kgmsg_q *gmsgq;

	msgq = *( (msg_q **) p );

	ASSERT_ERRNO_AND_EXIT ( msgq, E_INVALID_HANDLE );
	msgq = U2K_GET_ADR ( msgq, kthread_get_process (NULL) );

	gmsgq = msgq->handle;
	ASSERT_ERRNO_AND_EXIT ( gmsgq->id == msgq->id, E_INVALID_HANDLE );

	k_msgq_clean ( &gmsgq->mq );

	kthreadq_release_all ( &gmsgq->mq.thrq );

	k_free_unique_id ( gmsgq->id );

	kfree ( gmsgq );

	msgq->id = 0;
	msgq->handle = NULL;

	EXIT ( SUCCESS );
}

/*! Send message to queue or signal to thread */
int sys__msg_post ( void *p )
{
	/* parameters on thread stack */
	int dest_type;	/* MSG_QUEUE, MSG_THREAD or MSG_SIGNAL	*/
	void *dest;	/* (msg_q *) or (thread_t *)		*/
	msg_t *msg;	/* { type, size, data[0..size-1] }	*/
	uint flags;
	/* local variables */
	thread_t *thr;
	kthread_t *kthr, *new_kthr;
	kthrmsg_qs *thrmsg;
	kgmsg_q *kgmsgq;
	kmsg_q *kmsgq;
	msg_q *msgq;
	kmsg_t *kmsg;
	msg_t *cmsg;
	kprocess_t *proc;

	dest_type = *( (int *) p );	p += sizeof (int);
	dest = *( (void **) p );	p += sizeof (void *);
	msg = *( (msg_t **) p );	p += sizeof (msg_t *);
	flags = *( (uint *) p );

	ASSERT_ERRNO_AND_EXIT ( dest && msg, E_INVALID_HANDLE );

	dest = U2K_GET_ADR ( dest, kthread_get_process (NULL) );
	msg = U2K_GET_ADR ( msg, kthread_get_process (NULL) );

	if ( dest_type == MSG_THREAD || dest_type == MSG_SIGNAL )
	{
		thr = dest;
		kthr = kthread_get_descriptor ( thr );
		ASSERT_ERRNO_AND_EXIT ( kthr, E_DONT_EXIST );
		thrmsg = kthread_get_msgqs ( kthr );
		kmsgq = &thrmsg->msgq;
	}
	else if ( dest_type == MSG_QUEUE )
	{
		msgq = dest;
		kgmsgq = msgq->handle;
		ASSERT_ERRNO_AND_EXIT ( kgmsgq && kgmsgq->id == msgq->id,
					E_INVALID_HANDLE );
		kmsgq = &kgmsgq->mq;
	}
	else {
		EXIT ( E_INVALID_TYPE );
	}

	if ( dest_type == MSG_THREAD || dest_type == MSG_QUEUE )
	{
		/* send message to queue */
		if ( kmsgq->min_prio <= msg->type ) /* msg has required prio. */
		{
			kmsg = kmalloc ( sizeof (kmsg_t) + msg->size );
			ASSERT_ERRNO_AND_EXIT ( kmsg, E_NO_MEMORY );

			kmsg->msg.type = msg->type;
			kmsg->msg.size = msg->size;
			memcpy ( kmsg->msg.data, msg->data, msg->size );

			list_append ( &kmsgq->msgs, kmsg, &kmsg->list );

			/* is thread waiting for message? */
			if ( kthreadq_release ( &kmsgq->thrq ) )
				kthreads_schedule ();

			EXIT ( SUCCESS );
		}
		else { /* ignore message */
			EXIT ( E_IGNORED );
		}
	}

	/* must be MSG_SIGNAL */
	if ( thrmsg->sig_prio <= msg->type )
	{
		/* create thread that will service this signal */

		cmsg = kthread_create_private_storage ( kthr,
				sizeof (msg_t) + msg->size );
		cmsg->type = msg->type;
		cmsg->size = msg->size;
		memcpy ( cmsg->data, msg->data, msg->size );

		proc = kthread_get_process ( kthr );

		new_kthr = kthread_create (
			thrmsg->signal_handler,
			K2U_GET_ADR ( cmsg, proc ), proc->pi->exit, 0,
			kthread_get_prio ( kthr ) + 1, NULL, 0, 1, proc
		);
		ASSERT_ERRNO_AND_EXIT ( new_kthr, kthread_get_errno (NULL) );

		kthread_set_private_storage ( new_kthr, cmsg );

		SET_ERRNO ( SUCCESS );

		kthreads_schedule ();

		RETURN ( SUCCESS );
	}
	else { /* ignore signal */
		EXIT ( E_IGNORED );
	}
}

/*! Receive message from queue (global or from own thread message queue) */
int sys__msg_recv ( void *p )
{
	/* parameters on thread stack */
	int src_type;	/* MSG_QUEUE or MSG_THREAD 		*/
	void *src;	/* (msg_q *) or (thread_t *)		*/
	msg_t *msg;	/* { type, size, data[0..size-1] }	*/
	int type;	/* message type (identifier) */
	size_t size;	/* size of 'data' member */
	uint flags;
	/* local variables */
	kthread_t *kthr;
	kthrmsg_qs *thrmsg;
	kgmsg_q *kgmsgq;
	kmsg_q *kmsgq;
	msg_q *msgq;
	kmsg_t *kmsg;

	src_type = *( (int *) p );	p += sizeof (int);
	src = *( (void **) p );		p += sizeof (void *);
	msg = *( (msg_t **) p );	p += sizeof (msg_t *);
	type = *( (int *) p );		p += sizeof (int);
	size = *( (size_t *) p );	p += sizeof (size_t);
	flags = *( (uint *) p );

	ASSERT_ERRNO_AND_EXIT ( src && msg, E_INVALID_HANDLE );

	src = U2K_GET_ADR ( src, kthread_get_process (NULL) );
	msg = U2K_GET_ADR ( msg, kthread_get_process (NULL) );

	ASSERT_ERRNO_AND_EXIT ( src_type == MSG_THREAD || src_type == MSG_QUEUE,
				E_INVALID_TYPE );

	if ( src_type == MSG_THREAD )
	{
		kthr = kthread_get_active ();
		thrmsg = kthread_get_msgqs ( kthr );
		kmsgq = &thrmsg->msgq;
	}
	else { /* src_type == MSG_QUEUE */
		msgq = src;
		kgmsgq = msgq->handle;
		ASSERT_ERRNO_AND_EXIT ( kgmsgq && kgmsgq->id == msgq->id,
					E_INVALID_HANDLE );
		kmsgq = &kgmsgq->mq;
	}

	/* get first message from queue */
	kmsg = list_get ( &kmsgq->msgs, FIRST );

	if ( type != 0 ) /* type != 0 => search for first message 'type' */
		while ( kmsg && kmsg->msg.type != type )
			kmsg = list_get_next ( &kmsg->list );

	if ( kmsg ) /* have message */
	{
		if ( size < kmsg->msg.size )
		{
			msg->size = 0;
			EXIT ( E_TOO_BIG );
		}

		msg->type = kmsg->msg.type;
		msg->size = kmsg->msg.size;
		memcpy ( msg->data, kmsg->msg.data, msg->size );

		kmsg = list_remove ( &kmsgq->msgs, FIRST, &kmsg->list );
		ASSERT ( kmsg );
		kfree ( kmsg );

		EXIT ( SUCCESS );
	}
	else { /* queue empty! */
		if ( !( flags & IPC_WAIT ) )
			EXIT ( E_EMPTY );

		SET_ERRNO ( E_RETRY );
		/* block thread */
		kthread_enqueue ( NULL, &kmsgq->thrq );

		kthreads_schedule ();

		RETURN ( E_RETRY );
	}
}

/*! Delete all messages from queue */
void k_msgq_clean ( kmsg_q *kmsgq )
{
	kmsg_t *kmsg;

	kmsg = list_remove ( &kmsgq->msgs, FIRST, NULL );
	while ( kmsg )
	{
		kfree ( kmsg );
		kmsg = list_remove ( &kmsgq->msgs, FIRST, NULL );
	}
}

#endif /* MESSAGES */
