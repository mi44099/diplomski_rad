/*! EDF scheduling test example */

#include <api/stdio.h>
#include <api/thread.h>
#include <api/time.h>
#include <arch/processor.h>
#include <lib/types.h>

char PROG_HELP[] = "EDF scheduling demonstration example";

#define THR_NUM	4
#define TEST_DURATION	20 /* seconds */

void message ( int thread, char *action )
{
	time_t t;

	time_get ( &t );
	print ( "[%d:%d] Thread %d -- %s\n",
		t.sec, t.nsec/100000000, thread, action );
}

/* EDF thread */
static void edf_thread ( void *param )
{
	int thr_no, i, j;
	thr_no = (int) param;
	time_t period, deadline;

	i = thr_no;
	period.sec = thr_no * 1;
	period.nsec = 0;
	deadline.sec = thr_no * 1;
	deadline.nsec = 0;

	/*   DEADLINE = PERIOD / 2  */
//	deadline.sec = ( thr_no / 2 ) * 1;
//	deadline.nsec = 500000000 * ( thr_no % 2 );

	message ( thr_no, "EDF_SET" );
	edf_set ( deadline, period, EDF_SET, EDF_TERMINATE );

	for ( i = 0; ; i++ )
	{
		message ( thr_no, "EDF_WAIT" );
		if ( edf_wait () )
		{
			message ( thr_no, "Deadline missed, exiting!" );
			break;
		}

		message ( thr_no, "run" );
		for ( j = 1; j <= 38000000; j++ )
			memory_barrier();
	}

	message ( thr_no, "EDF_EXIT" );
	edf_exit ();
}

/* unimportant thread */
static void unimportant_thread ( void *param )
{
	time_t sleep;

	sleep.sec = 0;
	sleep.nsec = 100000000;
	while (1)
	{
		message ( 0, "unimportant thread" );
		delay ( &sleep );
	}
}

int edf ( char *args[] )
{
	thread_t thread[THR_NUM + 1];
	int i;
	time_t sleep;

	for ( i = 0; i < THR_NUM; i++ )
	{
		create_thread ( edf_thread, (void *) (i+1), SCHED_EDF,
				THR_DEFAULT_PRIO - 1, &thread[i] );
	}

	create_thread ( unimportant_thread, (void *) (i+1), SCHED_FIFO,
			THR_DEFAULT_PRIO - 2, &thread[i] );

//print( "ctrl_flags = %d\n", EDF_CONTINUE);
	print ( "Threads created, giving them %d seconds\n", TEST_DURATION );
	sleep.sec = TEST_DURATION;
	sleep.nsec = 0;
	delay ( &sleep );
	print ( "Test over - threads are to be canceled\n");

	for ( i = 0; i < THR_NUM + 1; i++ )
		cancel_thread ( &thread[i] );
	for ( i = 0; i < THR_NUM + 1; i++ )
		wait_for_thread ( &thread[i], IPC_WAIT );

	return 0;
}
