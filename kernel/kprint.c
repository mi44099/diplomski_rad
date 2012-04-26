/*! Formated printing on console (using 'device_t' interface) */
#define _KERNEL_

#include "kprint.h"
#include <kernel/devices.h>
#include <lib/string.h>

kdevice_t *k_stdout; /* initialized in startup.c */

/*!
 * Formated output to console (lightweight version of 'printf')
 * int kprint ( char *format, ... ) - defined in lib/print.h
 */
#define PRINT_NAME		kprint
#define PRINT_ATTRIBUT		KERNEL_FONT
#define DEVICE_SEND(TEXT,SZ)	k_device_send(&TEXT, SZ, PRINTSTRING, k_stdout);

#include <lib/print.h>
