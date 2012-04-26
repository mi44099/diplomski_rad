/*! simple shell interpreter */

#include <api/stdio.h>
#include <lib/string.h>
#include <api/time.h>
#include <api/syscall.h>
#include <api/thread.h>
#include <lib/types.h>

char PROG_HELP[] = "Simple command shell";

typedef struct _cmd_t_
{
	int (*func) ( char *argv[] );
	char *name;
	char *descr;
}
cmd_t;

#define MAXCMDLEN	72
#define MAXARGS		10
#define PROG_LIST_SIZE	1000
#define INFO_SIZE	1000

static char s_stdout[MAXCMDLEN];
static char s_stdin[MAXCMDLEN];

static int help ();
static int clear ();
static int sysinfo ( char *args[] );
static int set ( char *args[] );

static cmd_t sh_cmd[] =
{
	{ help, "help", "help - list available commands" },
	{ clear, "clear", "clear - clear screen" },
	{ sysinfo, "sysinfo", "system information; usage: sysinfo [options]" },
	{ set, "set", "change shell settings; "
		"usage: set stdin|stdout [device]" },
	{ NULL, "" }
};

int shell ( char *args[] )
{
	char cmd[MAXCMDLEN + 1];
	int i, key;
	time_t t;
	int argnum;
	char *argval[MAXARGS + 1];
	thread_t thr;

	print ( "\n*** Simple shell interpreter ***\n\n" );
	help ();

	t.sec = 0;
	t.nsec = 100000000; /* 100 ms */

	strcpy ( s_stdout, U_STDOUT );
	strcpy ( s_stdin, U_STDIN );

	while (1)
	{
		new_cmd:
		print ( "\n> " );

		i = 0;
		memset ( cmd, 0, MAXCMDLEN );

		/* get command - get chars until new line is received */
		while ( i < MAXCMDLEN )
		{
			key = get_char ();

			if ( !key )
			{
				delay ( &t );
				continue;
			}

			if ( key == '\n' || key == '\r')
			{
				if ( i > 0 )
					break;
				else
					goto new_cmd;
			}

			switch ( key )
			{
			case '\b':
				if ( i > 0 )
				{
					cmd[--i] = 0;
					print ( "%c", key );
				}
				break;

			default:
				print ( "%c", key );
				cmd[i++] = key;
				break;
			}
		}
		print ( "\n" );

		/* parse command line */
		argnum = 0;
		for(i = 0; i < MAXCMDLEN && cmd[i]!=0 && argnum < MAXARGS; i++)
		{
			if ( cmd[i] == ' ' || cmd[i] == '\t')
				continue;

			argval[argnum++] = &cmd[i];
			while ( cmd[i] && cmd[i] != ' ' && cmd[i] != '\t'
				&& i < MAXCMDLEN )
				i++;

			cmd[i] = 0;
		}
		argval[argnum] = NULL;

		/* match command to shell command */
		for ( i = 0; sh_cmd[i].func != NULL; i++ )
		{
			if ( strcmp ( argval[0], sh_cmd[i].name ) == 0 )
			{
				if ( sh_cmd[i].func ( argval ) )
					print ( "\nProgram returned error!\n" );

				goto new_cmd;
			}
		}

		/* not shell command; start given program by calling kernel */
		if ( !start_program ( argval[0], &thr, (void *) &argval[0],
			0, THR_DEFAULT_PRIO ) )
		{
			if ( argnum < 2 || argval[argnum-1][0] != '&' )
				wait_for_thread ( &thr, IPC_WAIT );

			goto new_cmd;
		}

		if ( strcmp ( argval[0], "quit" ) == 0 ||
			strcmp ( argval[0], "exit" ) == 0 )
			break;

		/* not program kernel or shell knows about it - report error! */
		print ( "Invalid command!" );
	}

	print ( "Exiting from shell\n" );

	return 0;
}

static int help ()
{
	int i;

	print ( "Shell commands: " );
	for ( i = 0; sh_cmd[i].func != NULL; i++ )
		print ( "%s ", sh_cmd[i].name );
	print ( " quit/exit\n" );

	return 0;
}

static int clear ()
{
	return clear_screen ();
}

static int sysinfo ( char *args[] )
{
	char info[INFO_SIZE];

	syscall ( SYSINFO, &info, INFO_SIZE, args );

	print ( "%s\n", info );

	return 0;
}

static int set ( char *args[] )
{
	if ( args[1] == NULL ||
		( strcmp ( args[1], "stdin"  ) &&
		  strcmp ( args[1], "stdout" )		)	)
	{
		print ( "'set' usage: set stdin|stdout [device]\n" );
	}
	else if ( args[2] == NULL )
	{
		/* print current cofiguration */
		print ( "console: stdout = %s, stdin = %s\n",
			s_stdout, s_stdin );
	}
	else if ( strcmp ( args[1], "stdin" ) == 0 )
	{
		if ( strcmp ( args[2], s_stdin ) == 0 )
		{
			print ( "Given stdin (%s) is already in use!\n",
				args[2] );
		}
		else if ( change_default_stdin ( args[2] ) )
		{
			print ( "Error in changing stdin to %s\n", args[2] );
		}
		else {
			strcpy ( s_stdin, args[2] );
		}
	}
	else if ( strcmp ( args[1], "stdout" ) == 0 )
	{
		if ( strcmp ( args[2], s_stdout ) == 0 )
		{
			print ( "Given stdout (%s) is already in use!\n",
				args[2] );
		}
		else if ( change_default_stdout ( args[2] ) )
		{
			print ( "Error in changing stdout to %s\n", args[2] );
		}
		else {
			strcpy ( s_stdout, args[2] );
		}
	}
	else {
		/* must not get here */
		print ( "Internal shell error!\n" );
	}

	return 0;
}