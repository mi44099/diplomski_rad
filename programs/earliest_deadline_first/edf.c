/*! EDF scheduling test example */

#include <api/stdio.h>
#include <api/thread.h>
#include <api/time.h>
#include <arch/processor.h>

char PROG_HELP[] = "EDF scheduling demonstration example";

#define THR_NUM	1


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

	print ( "Thread %d calling EDF_SET\n", thr_no );
	edf_set ( deadline, period, EDF_SET );

	for ( i = 0; i < 3; i++ )
	{
		print ( "Thread %d calling EDF_WAIT\n", thr_no );
		edf_wait ();

		print( "Thread %d running\n", thr_no );
		for ( j = 1; j <= 10000000; j++ )
			memory_barrier();
	}

	print ( "Thread %d calling EDF_EXIT\n", thr_no );
	edf_exit ();
}

int edf ( char *args[] )
{
	thread_t thread[THR_NUM + 1];
	int i;

	for ( i = 0; i < THR_NUM; i++ )
	{
		create_thread ( edf_thread, (void *) (i+1), SCHED_EDF,
				THR_DEFAULT_PRIO - 1, &thread[i] );
	}


	for ( i = 0; i < THR_NUM; i++ )
		wait_for_thread ( &thread[i], IPC_WAIT );

	return 0;
}
