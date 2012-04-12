/*! Interrupt handling - 'arch' layer (only basic operations) */

#pragma once

#ifndef ASM_FILE

/*! (Hardware) Interrupt controller interface */
typedef struct _interrupt_controller_
{
	void (*init) ();
	void (*disable_irq) ( unsigned int irq );
	void (*enable_irq) ( unsigned int irq );
	void (*at_exit) ( unsigned int irq );

	char *(*int_descr) ( unsigned int irq );
}
arch_ic_t;

void arch_init_interrupts ();
void arch_register_interrupt_handler ( unsigned int inum, void *handler,
				       void *device );
void arch_unregister_interrupt_handler ( unsigned int irq_num, void *handler,
					 void *device );

void arch_return_to_thread (); /* defined in interrupts.S */

extern void (*arch_irq_enable) ( unsigned int irq );
extern void (*arch_irq_disable) ( unsigned int irq );

int arch_new_mode ();
int arch_prev_mode ();

#endif /* ASM_FILE */

/* Programmable Interrupt controllers (currently implemented only one, i8259) */
#include <arch/devices/i8259.h>

/* Constants */
#define KERNEL_MODE		0
#define USER_MODE		-1

#define INT_STF			12	/* Stack Fault */
#define INT_GPF			13	/* General Protection Fault */

#define SOFTWARE_INTERRUPT	SOFT_IRQ
#define INTERRUPTS		NUM_IRQS
