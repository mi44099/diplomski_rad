/*! 'serial port' module (UART) */
#ifdef UART

#define _UART_C_
#include "uart.h"

#include <arch/io.h>
#include <arch/interrupts.h>
#include <lib/types.h>
#include <devices/devices.h>
#define _KERNEL_
#include <kernel/errno.h>

/*!
 * Identify UART chip
 * - algorithm taken from:
 *   http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming
 */
static int identify_UART ( arch_uart_t *up )
{
	int test;

	if ( up->uart_type )
		return up->uart_type;

	outb ( COM1_BASE + FCR, 0xE7 );
	test = inb ( COM1_BASE + IIR );
	if ( test & 0x40 )
	{
		if ( test & 0x80 )
		{
			if ( test & 0x20 )
				up->uart_type = UT16750;
			else
				up->uart_type = UT16550A;
		}
		else {
			up->uart_type = UT16550;
		}
	}
	else {
		outb ( COM1_BASE + SR, 0x2A );
		test = inb ( COM1_BASE + SR );
		if ( test == 0x2A )
			up->uart_type = UT16450;
		else
			up->uart_type = UT8250;
	}

	return up->uart_type;
}

/*! Initialize UART device */
int uart_init ( uint flags, void *params, device_t *dev )
{
	arch_uart_t *up;

	/* set default parameters */
	up = dev->params;

	/* check chip */
	identify_UART ( up );

	if ( up->uart_type )
		return 0; /* already initialized */

	return uart_config ( dev, &up->params );
}

/*! Set UART configuration */
static int uart_config ( device_t *dev, uart_t *params )
{
	arch_uart_t *up;
	uint8 setting;

	ASSERT ( dev );

	ASSERT_ERRNO_AND_RETURN ( params->data_bits >= 5 || params->data_bits<=8,
				  E_INVALID_ARGUMENT );
	ASSERT_ERRNO_AND_RETURN ( params->parity >= 0 || params->parity <= 7,
				  E_INVALID_ARGUMENT );
	ASSERT_ERRNO_AND_RETURN ( params->stop_bit >= STOPBIT_1 ||
				  params->stop_bit <= STOPBIT_15,
				  E_INVALID_ARGUMENT );
	ASSERT_ERRNO_AND_RETURN ( params->mode == UART_BYTE ||
				  params->mode == UART_STREAM,
				  E_INVALID_ARGUMENT );

	up = dev->params;
	up->params = *params;

	/* first disable interrupts */
	outb ( up->port + IER, 0 );

	/* clear FIFO (set FCR) */
	if ( up->uart_type > UT8250 )
	{
		setting = FCR_ENABLE | FCR_CLEAR;

		if ( params->mode == UART_BYTE )
			setting |= FCR_BYTE_MODE;
		else
			setting |= FCR_STREAM_MODE;

		if ( up->uart_type == UT16750 )
			setting |= FCR_64BYTES;

		outb ( up->port + FCR, setting );
	}

	/* load divisior */
	outb ( up->port + LCR, LCR_DLAB_ON ); /* set DLAB=1 */
	outb ( up->port + DLL, up->params.speed & 0xff ); /* Low Byte */
	outb ( up->port + DLM, up->params.speed >> 8 );  /* High Byte */
	outb ( up->port + LCR, LCR_DLAB_OFF ); /* set DLAB=0 */

	/* set LCR */
	setting = up->params.data_bits - 5;
	setting |= up->params.parity | up->params.stop_bit | LCR_DLAB_OFF;
	if ( up->uart_type >= UT16550 )
		setting |= LCR_BREAK;

	/* set MCR */
	outb ( up->port + MCR, MCR_DEFAULT );

	/* set IER */
	outb ( up->port + IER, IER_DEFAULT );

	/* "clear" software buffers */
	up->inf = up->inl = up->insz = 0;
	up->outf = up->outl = up->outsz = 0;

	//up->initialized = TRUE;

	return 0;
}

/*! Disable UART device */
int uart_destroy ( uint flags, void *params, device_t *dev )
{
	arch_uart_t *up;

	ASSERT ( dev );

	up = dev->params;

	/* clear IER - disable interrupt generation */
	outb ( up->port + IER, IER_DISABLE );

	return 0;
}

/*! Interrupt handler for UART device */
static void uart_interrupt_handler ( int irq_num, void *device )
{
	device_t *dev;
	arch_uart_t *up;
	uint8 iir;
	int brk, rcv, snd;

	dev = device;
	up = dev->params;
	rcv = snd = brk = FALSE;

	while (1)
	{
		iir = inb ( up->port + IIR );

		if ( !( iir & IIR_INT_PENDING ) )
			return; /* no interrupt pending from this device */

		if ( iir & IIR_TIMEOUT )
			brk = TRUE;

		if ( iir & IIR_LINE )
		{
			if ( inb ( up->port + LSR ) & LSR_DATA_READY )
				rcv = TRUE;
			else if ( inb ( up->port + LSR ) & LSR_THR_EMPTY )
				snd = TRUE;
			else if ( inb ( up->port + LSR ) & LSR_BREAK )
				snd = TRUE;
			/* else TODO: handle errors */
		}

		if ( rcv || brk || ( iir & IIR_RECV_DATA ) )
		{
			/* read data from UART to software buffer */
			uart_read ( up );
			rcv = TRUE;
			continue;
		}

		if ( snd || iir & IIR_THR_EMPTY )
		{
			/* if there is data in software buffer send them */
			uart_write ( up );
			snd = TRUE;
			continue;
		}

		if ( iir & IIR_MODEM )
		{
			/* TODO: handle modem interrupts */
		}
	}

	/* TODO: do something with rcv, snd, brk ? */
}

/*! Read data from UART to software buffer */
static void uart_read ( arch_uart_t *up )
{
	while ( inb ( up->port + LSR ) & LSR_DATA_READY )
	{
		up->inbuff[up->inl] = inb ( up->port + RBR );
		INC_MOD ( up->inl, BUFFER_SIZE );

		up->insz++;
		if ( up->insz > BUFFER_SIZE )
		{
			/* overwrite oldes char in buffer */
			up->insz--;
			INC_MOD ( up->inf, BUFFER_SIZE );
		}
	}
}

/*! If there is data in software buffer send them to UART */
static void uart_write ( arch_uart_t *up )
{
	while ( up->outsz > 0 && inb ( up->port + LSR ) & LSR_THR_EMPTY )
	{
		outb ( up->port + THR, up->outbuff[up->outf] );
		INC_MOD ( up->outf, BUFFER_SIZE );
		up->outsz--;
	}
}

/*! Send data to UART device (through software buffer) */
static int uart_send ( void *data, size_t size, uint flags, device_t *dev )
{
	arch_uart_t *up;
	uint8 *d, pchar;
	struct _param_ {
		int attr;
		char text[1];
	}
	*param;

	ASSERT ( dev );

	if ( flags & UART_SETCONF )
		return uart_config ( dev, (uart_t *) data );

	/* send */
	flags &= ~UART_SETCONF;

	up = dev->params;
	d = data;

	/* console switch? emulate stdout: CLEAR and GOTOXY: both send '\n' */
	switch ( flags )
	{
		case PRINTSTRING:
			param = data;
			d = (uint8 *) &param->text[0];
			break;

		case CLEAR:
		case GOTOXY:
			/* go to new line */
			pchar = '\n';
			d = &pchar;
			size = 1;
			break;

		default:
			/* send raw bytes */
			break;
	}

	/* first, copy to software buffer */
	while ( size > 0 && up->outsz < BUFFER_SIZE )
	{
		if ( *d == 0 && flags == PRINTSTRING )
			break;

		up->outbuff[up->outl] = *d++;

		INC_MOD ( up->outl, BUFFER_SIZE );

		up->outsz++;
		size--;
	}

	/* second, copy from software buffer to uart */
	uart_write ( up );

	return size; /* 0 if all sent, otherwise not send part length */
}

/*! Read from UART (using software buffer) */
static int uart_recv ( void *data, size_t size, uint flags, device_t *dev )
{
	arch_uart_t *up;
	uint8 *d;
	int i;

	ASSERT ( dev );

	up = dev->params;

	if ( flags & UART_GETCONF )
	{
		if ( size < sizeof (uart_t) )
			return -1;

		*( (uart_t *) data ) = up->params;

		return sizeof (uart_t);
	}

	/* else = flags ==  UART_RECV */

	/* first, copy from uart to software buffer */
	uart_read ( up );

	/* second, copy from software buffer to data */
	d = data;
	i = 0;
	while ( i < size && up->insz > 0 )
	{
		d[i] = up->inbuff[up->inf];

		INC_MOD ( up->inf, BUFFER_SIZE );

		up->insz--;
		i++;
	}

	return i; /* bytes read */
}


/*! COM1 device & parameters */
static arch_uart_t com1_params = (arch_uart_t)
{
	.uart_type = UNDEFINED,
	.params = UART_DEFAULT_SETTING,
	.port = COM1_BASE,
	.inf = 0, .inl = 0, .insz = 0,
	.outf = 0, .outl = 0, .outsz = 0
};

/*! uart as device_t */
device_t uart_com1 = (device_t)
{
	.dev_name = "COM1",

	.irq_num = 	IRQ_COM1,
	.irq_handler =	uart_interrupt_handler,

	.init =		uart_init,
	.destroy =	uart_destroy,
	.send =		uart_send,
	.recv =		uart_recv,

	.flags = 	DEV_TYPE_SHARED,
	.params = 	&com1_params
};

#endif /* UART */
