/*! EDF scheduling test example */

#include <api/stdio.h>
#include <api/thread.h>
#include <api/time.h>
#include <arch/processor.h>

char PROG_HELP[] = "Thread demonstration example: create several threads that "
		   "perform simple iterations and print basic info.";

#define THR_NUM	3
#define TEST_DURATION	10 /* seconds */


/* example threads */
static void edf_thread ( void *param )
{
	int thr_no, i, j;
	time_t sleep;
	thr_no = (int) param;
	time_t period, deadline;

	i = thr_no;
	period.sec = thr_no * 4;
	period.nsec = 0;
	deadline.sec = thr_no * 2;
	deadline.nsec = 0;

	print ( "EDF thread %d setting\n", thr_no );
	edf_set ( deadline, period, EDF_SET );

	while ( i-- > 0 ) 
	{
		print ( "Thread %d call EDF_WAIT\n", thr_no );
		edf_wait ( deadline, period, EDF_WAIT );
		
		print( "Thread %d is running\n", thr_no );
		for ( j = 1; j <= 100000000; j++ )
		{
			if ( j % 10000000 == 0 ) print( "thread %d = %d % \n", thr_no, j / 1000000 );
		}

	}

	print ( "EDF thread %d exiting\n", thr_no );
	edf_exit ( deadline, period, EDF_EXIT );
}

int edf ( char *args[] )
{
	thread_t thread[THR_NUM + 1];
	int i;
	time_t sleep;

	for ( i = 1; i <= THR_NUM; i++ )
	{
		create_thread ( edf_thread, (void *) i, SCHED_EDF, THR_DEFAULT_PRIO - 1, &thread[i] );
	}

	print ( "Threads created!!\n\n" );

	for ( i = 1; i <= THR_NUM; i++ ) {
		wait_for_thread ( &thread[i], IPC_WAIT );
	}
	for ( i = 1; i <= THR_NUM; i++ )
		print ( "Thread %d\n", i );

	return 0;
}
