/*! Generate segmentation fault */

#include <api/stdio.h>

char PROG_HELP[] = "Generate segmentation fault";

/* detect memory faults (qemu do not detect segment violations!) */

int segm_fault ( char *argv[] )
{
	unsigned int *p;
	unsigned int i, j=0;

	print ( "Before segmentation fault\n" );

	for ( i = 2; i < 32; i++ )
	{
		p = (unsigned int *) (1 << i);
		print ( "[%x]=%d\n", p, *p );
		j+= *p;
	}

	print ( "After excpected segmentation fault, j=%d\n", j );

	return 0;
}
