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
	int thr_no, i, j, k = 2;
	time_t sleep;
	thr_no = (int) param;
	time_t period, deadline;

	i = thr_no;
	period.sec = thr_no * 4;
	period.nsec = 0;
	deadline.sec = thr_no * 2;
	deadline.nsec = 0;

	print ( "EDF thread %d setting", thr_no );
	edf_set ( deadline, period, EDF_SET );

	while ( k-- > 0 ) 
	{
		print ( "\nThread %d call EDF_WAIT\n", thr_no );
		edf_wait ( deadline, period, EDF_WAIT );

		print( "\nThread %d is running", thr_no );
		for ( j = 1; j <= thr_no * 6 * 10000000; j++ );
	}

	print ( "\nEDF thread %d exiting\n", thr_no );
	edf_exit ( deadline, period, EDF_EXIT );
}

int edf ( char *args[] )
{
	thread_t thread[THR_NUM + 1];
	int i;
	time_t sleep;

	sleep.sec = 1;
	sleep.nsec = 0;

	for ( i = 1; i <= THR_NUM; i++ )
	{
		create_thread ( edf_thread, (void *) i, SCHED_EDF, THR_DEFAULT_PRIO - 1, &thread[i] );
		//delay( &sleep );
	}

	//print ( "Threads created!!\n\n" );

	
	for ( i = 1; i <= THR_NUM; i++ )
		wait_for_thread ( &thread[i], IPC_WAIT );

	while(1);

	for ( i = 1; i <= THR_NUM; i++ )
		print ( "Thread %d\n", i );

//	while(1);

	return 0;
}
