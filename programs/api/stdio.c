/*! Printing on stdout, reading from stdin */

#include "stdio.h"
#include <api/syscall.h>
#include <api/prog_info.h>
#include <lib/string.h>
#include <lib/types.h>

extern prog_info_t pi; /* defined in api/prog_info.c */

/*! Change standard input device */
int change_stdin ( char *new_stdin )
{
	void *new_dev;

	syscall ( DEVICE_OPEN, new_stdin, &new_dev );

	if ( new_dev )
	{
		/* syscall ( DEVICE_CLOSE, pi.stdin );
		 * ->this should be system wide !*/
		pi.stdin = new_dev;
	}

	return !new_dev;
}

/*! Change standard output device */
int change_stdout ( char *new_stdout )
{
	void *new_dev;

	syscall ( DEVICE_OPEN, new_stdout, &new_dev );

	if ( new_dev )
	{
		/* syscall ( DEVICE_CLOSE, pi.stdin );
		 * ->this should be system wide !*/
		pi.stdout = new_dev;
	}

	return !new_dev;
}

/*! Change default standard input device (for current and new programs) */
int change_default_stdin ( char *new_stdin )
{
	void *new_dev;

	syscall ( SET_DEFAULT_STDIN, new_stdin, &new_dev );

	if ( new_dev )
	{
		/* syscall ( DEVICE_CLOSE, pi.stdin );
		 * ->this should be system wide !*/
		pi.stdin = new_dev;
	}

	return !new_dev;
}

/*! Change default standard output device (for current and new programs) */
int change_default_stdout ( char *new_stdout )
{
	void *new_dev;

	syscall ( SET_DEFAULT_STDOUT, new_stdout, &new_dev );

	if ( new_dev )
	{
		/* syscall ( DEVICE_CLOSE, pi.stdin );
		 * ->this should be system wide !*/
		pi.stdout = new_dev;
	}

	return !new_dev;
}

/*! Get input from "standard input" */
inline int get_char ()
{
	int c = 0;

	syscall ( DEVICE_RECV, (void *) &c, 1, ONLY_ASCII, pi.stdin );

	return c;
}

/*! Erase screen (if supported by stdout device) */
inline int clear_screen ()
{
	return syscall ( DEVICE_SEND, NULL, 0, CLEAR, pi.stdout );
}

/*! Move cursor to given position (if supported by stdout device) */
inline int goto_xy ( int x, int y )
{
	int p[2];

	p[0] = x;
	p[1] = y;

	return syscall ( DEVICE_SEND, &p, 2 * sizeof (int), GOTOXY, pi.stdout );
}

/*!
 * Formated output to console (lightweight version of 'printf')
 * int print ( char *format, ... ) - defined in lib/print.h
 */
#define PRINT_FUNCTION_NAME	print
#define PRINT_ATTRIBUT		USER_FONT
#define DEVICE_SEND(TEXT,SZ)	syscall ( DEVICE_SEND, &TEXT, SZ,	\
					  PRINTSTRING, pi.stdout );
#include <lib/print.h>
