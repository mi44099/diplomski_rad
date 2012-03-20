/*! Devices - common interface implementation */
#define _KERNEL_

#include "devices.h"
#include <kernel/errno.h>
#include <kernel/memory.h>
#include <arch/interrupts.h>
#include <lib/string.h>

static list_t devices;

/*! Init 'device' subsystem */
int k_devices_init ()
{
	extern device_t DEVICES_DEV; /* defined in arch/devices, Makefile */
	device_t *dev[] = { DEVICES_DEV_PTRS, NULL };
	kdevice_t *kdev;
	int iter;

	list_init ( &devices );

	for ( iter = 0; dev[iter] != NULL; iter++ )
	{
		kdev = k_device_add ( dev[iter] );
		k_device_init ( kdev, 0, NULL, NULL );
	}

	return 0;
}

/*! Add new device to system */
kdevice_t *k_device_add ( device_t *dev )
{
	kdevice_t *kdev;

	ASSERT ( dev );

	kdev = kmalloc ( sizeof (kdevice_t) );
	ASSERT ( kdev );

	kdev->dev = *dev;
	kdev->open = 0;

	list_append ( &devices, kdev, &kdev->list );

	return kdev;
}

/*! Initialize device (and call its initializer, if set) */
int k_device_init ( kdevice_t *kdev, int flags, void *params, void *callback )
{
	int retval = 0;

	ASSERT ( kdev );

	if ( flags )
		kdev->dev.flags = flags;

	if ( params )
		kdev->dev.params = params;

	kdev->locked = FALSE;
	kthreadq_init ( &kdev->thrq );

	if ( kdev->dev.init )
		retval = kdev->dev.init ( flags, params, &kdev->dev );

	if ( !retval && kdev->dev.irq_handler )
	{
		(void) arch_register_interrupt_handler ( kdev->dev.irq_num,
							 kdev->dev.irq_handler,
							 &kdev->dev );
		arch_irq_enable ( kdev->dev.irq_num );
	}

	if ( callback )
		kdev->dev.callback = callback;

	return retval;
}

/*! Remove device from list of devices */
int k_device_remove ( kdevice_t *kdev )
{
	ASSERT ( kdev );

	if ( kdev->dev.irq_num != -1 )
		arch_irq_disable ( kdev->dev.irq_num );

	if ( kdev->dev.irq_handler )
		arch_unregister_interrupt_handler ( kdev->dev.irq_num,
						    kdev->dev.irq_handler,
						    &kdev->dev );
	if ( kdev->dev.destroy )
		kdev->dev.destroy ( kdev->dev.flags, kdev->dev.params,
				    &kdev->dev );

	list_remove ( &devices, FIRST, &kdev->list );

	kfree ( kdev );

	return 0;
}

/*! Send data to device */
int k_device_send ( void *data, size_t size, int flags, kdevice_t *kdev )
{
	int retval;

	if ( kdev->dev.send )
		retval = kdev->dev.send ( data, size, flags, &kdev->dev );
	else
		retval = -1;

	return retval;
}

/*! Read data from device */
int k_device_recv ( void *data, size_t size, int flags, kdevice_t *kdev )
{
	int retval;

	if ( kdev->dev.recv )
		retval = kdev->dev.recv ( data, size, flags, &kdev->dev );
	else
		retval = -1;

	return retval;
}

/*! Open device with 'name' (for exclusive use, if defined) */
kdevice_t *k_device_open ( char *name )
{
	kdevice_t *kdev;

	kdev = list_get ( &devices, FIRST );
	while ( kdev )
	{
		if ( !strcmp ( name, kdev->dev.dev_name ) )
		{
			if ( kdev->dev.flags & DEV_TYPE_NOTSHARED )
			{
				if ( kdev->open )
				{
					return NULL; /* in use */
				}
				else {
					kdev->open = TRUE;
					return kdev;
				}
			}
			else {
				kdev->open = TRUE;
				return kdev;
			}
		}

		kdev = list_get_next ( &kdev->list );
	}

	return NULL;
}

/*! Close device (close exclusive use, if defined) */
void k_device_close ( kdevice_t *kdev )
{
	kdev->open = FALSE;
}

/*! Lock device */
int k_device_lock ( kdevice_t *dev, int wait )
{
	if ( !wait && dev->locked )
		return -1;

	if ( dev->locked )
	{
		kthread_enqueue ( NULL, &dev->thrq );
		kthreads_schedule ();
	}

	dev->locked = TRUE;

	return 0;
}

/*! Unlock device */
int k_device_unlock ( kdevice_t *dev )
{
	if ( kthreadq_release ( &dev->thrq ) )
		kthreads_schedule ();
	else
		dev->locked = FALSE;

	return 0;
}

/*! syscall wrappers -------------------------------------------------------- */

int sys__device_send ( void *p )
{
	void *data;
	size_t size;
	int flags;
	kdevice_t *dev;

	data = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	p += sizeof (void *);

	size = *( (size_t *) p );
	p += sizeof (size_t);

	flags = *( (int *) p );
	p += sizeof (int);

	dev = *( (void **) p );

	return k_device_send ( data, size, flags, dev );
}

int sys__device_recv ( void *p )
{
	void *data;
	size_t size;
	int flags;
	kdevice_t *dev;

	data =  U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );
	p += sizeof (void *);

	size = *( (size_t *) p );
	p += sizeof (size_t);

	flags = *( (int *) p );
	p += sizeof (int);

	dev = *( (void **) p );

	return k_device_recv ( data, size, flags, dev );
}

int sys__device_open ( void *p )
{
	char *dev_name;
	void **dev;

	dev_name = U2K_GET_ADR ( *( (char **) p ), kthread_get_process (NULL) );
	p += sizeof (char *);

	dev = U2K_GET_ADR ( *( (void **) p ), kthread_get_process (NULL) );

	*dev = k_device_open ( dev_name );

	return *dev == NULL;
}

int sys__device_close ( void *p )
{
	kdevice_t *kdev;

	kdev = *( (void **) p );

	k_device_close ( kdev );

	return 0;
}

int sys__device_lock ( void *p )
{
	kdevice_t *dev;
	int wait;

	dev = *( (void **) p ); p += sizeof (void *);
	wait = *( (int *) p );

	return k_device_lock ( dev, wait );
}

int sys__device_unlock ( void *p )
{
	kdevice_t *dev;

	dev = *( (void **) p );

	return k_device_unlock ( dev );
}
