/*! Formated print on console (using provided interface) */

/*
 * "print.c" must define constants: PRINT_NAME, PRINT_ATTRIBUT and DEVICE_SEND
 *
 * For example, kernel/print.c, before including this header must define:
 *
 * #define PRINT_FUNCTION_NAME		kprint
 * #define PRINT_ATTRIBUT	KERNEL_FONT
 * #define DEVICE_SEND(TEXT,SZ)	k_device_send(&TEXT, SZ, PRINTSTRING, k_stdout);
 *
 */

#define	TEXTSZ	80
struct _text_ {
	int attr;
	char text[TEXTSZ];
};

/*
 * macro that uses 'text' and 'ind' variables defined in next PRINT_NAME for
 * saving character to buffer for later printing on device
 */
#define	PRINT2BUFFER(CHAR, BUF_SIZE)			\
do {							\
	text.text[ind++] = (char) (CHAR);		\
	if ( ind == (BUF_SIZE)-1 )			\
	{						\
		text.text[ind] = 0;			\
		DEVICE_SEND ( text, BUF_SIZE );		\
		ind = 0;				\
	}						\
}							\
while (0)

/*! Formated output to console (lightweight version of 'printf') */
int PRINT_FUNCTION_NAME ( char *format, ... )
{
	char **arg = &format;
	int c;
	char buf[20];
	int ind = 0;
	struct _text_ text;

	text.attr = PRINT_ATTRIBUT;

	if ( !format )
		return 0;

	arg++; /* first argument after 'format' (on stack) */

	while ( (c = *format++) != 0 )
	{
		if ( c != '%' )
		{
			PRINT2BUFFER ( c, TEXTSZ );
		}
		else {
			char *p;

			c = *format++;
			switch ( c ) {
			case 'd':
			case 'u':
			case 'x':
			case 'X':
				itoa ( buf, c, *((int *) arg++) );
				p = buf;
				while ( *p )
					PRINT2BUFFER ( *p++, TEXTSZ );
				break;

			case 's':
				p = *arg++;
				if ( !p )
					p = "(null)";

				while ( *p )
					PRINT2BUFFER ( *p++, TEXTSZ );
				break;

			default: /* assuming c=='c' */
				PRINT2BUFFER ( *((int *) arg++), TEXTSZ );
				break;
			}
		}
	}

	PRINT2BUFFER ( 0, ind+1 );

	return 0;
}

#undef	PRINT2BUFFER
#undef	PRINT_NAME
#undef	PRINT_ATTRIBUT
#undef	DEVICE_SEND
#undef	TEXTSZ
