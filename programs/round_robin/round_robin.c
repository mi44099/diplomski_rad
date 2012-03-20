/*! Round Robin scheduling test example */

#include <api/stdio.h>
#include <api/thread.h>
#include <api/time.h>
#include <arch/processor.h>

char PROG_HELP[] = "Thread demonstration example: create several threads that "
		   "perform simple iterations and print basic info.";

#define THR_NUM	3
#define INNER_LOOP_COUNT 100000
#define TEST_DURATION	10 /* seconds */

static int iters[THR_NUM];

/* example threads */
static void rr_thread ( void *param )
{
	int i, j, thr_no;

	thr_no = (int) param;

	print ( "RR thread %d starting\n", thr_no );
	for ( i = 1; ; i++ )
	{
		for ( j = 0; j < INNER_LOOP_COUNT; j++ )
			memory_barrier ();

		iters[thr_no]++;
	}
	print ( "RR thread %d exiting\n", thr_no );
}

int round_robin ( char *args[] )
{
	thread_t thread[THR_NUM];
	int i;
	time_t sleep;

	for ( i = 0; i < THR_NUM; i++ )
	{
		iters[i] = 0;
		create_thread ( rr_thread, (void *) i,
				SCHED_RR, THR_DEFAULT_PRIO - 1, &thread[i] );
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
	for ( i = 0; i < THR_NUM; i++ )
		print ( "Thread %d, count=%d\n", i, iters[i] );

	return 0;
}
