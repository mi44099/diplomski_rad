/*! EDF scheduling test example */

#include <api/stdio.h>
#include <api/thread.h>
#include <api/time.h>
#include <arch/processor.h>

char PROG_HELP[] = "EDF scheduling demonstration example";

#define THR_NUM	3
#define TEST_DURATION	10 /* seconds */

void message ( int thread, char *action )
{
	time_t t;

	time_get ( &t );
	print ( "[%d:%d] Thread %d -- %s\n",
		t.sec, t.nsec/100000000, thread, action );
}

/* example threads */
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

	message ( thr_no, "EDF_SET" );
	edf_set ( deadline, period, EDF_SET );

	for ( i = 0; ; i++ )
	{
		message ( thr_no, "EDF_WAIT" );
		edf_wait ();

		message ( thr_no, "run" );
		for ( j = 1; j <= 60000000; j++ )
			memory_barrier();
	}

	message ( thr_no, "EDF_EXIT" );
	edf_exit ();
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

	print ( "Threads created, giving them %d seconds\n", TEST_DURATION );
	sleep.sec = TEST_DURATION;
	sleep.nsec = 0;
	delay ( &sleep );
	print ( "Test over - threads are to be canceled\n");

	for ( i = 0; i < THR_NUM; i++ )
		cancel_thread ( &thread[i] );
	for ( i = 0; i < THR_NUM; i++ )
		wait_for_thread ( &thread[i], IPC_WAIT );

	return 0;
}
