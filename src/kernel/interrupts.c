/*
 * Cheetah v1.0   Copyright Marlet Limited 2005
 *
 * File:	interrupts.c
 * Author:	James Smith
 * Date:	10-Jan-2005
 * Description: Main interrupt handling rountines
 *
 */

#include "kernel.h"


static DeviceList *DeviceIRQListHead[16];	/* 16 IRQs supported (0-15) */


void IRQHandler(int IRQ) {
	con_WriteChar( 'A' );
}

void proper_IRQHandler(int IRQ) {
	/* This is called from the generic ASM interrupt handler that is
	   registered in the IDT. Some registers have been pushed, and
	   the IRQ number has been put on the stack so it appears here
	   as an input parameter. */

	/* Code here should be small and quick top half handler. Queue
	   the interrupt somewhere, and then let the bottom half of the
	   handler finish the job off later on. */

	/* Check the input parameter */
	if ((IRQ < 0) || (IRQ > 15)) {
		/* Bad IRQ number. Bail */
		return;
	}

	/* Find the device drivers that have registered to receive this
	   IRQ event, and inform each one that the interrupt has occurred. */

	/* Get the head of the linked list of devices for this IRQ */
	DeviceList *list = DeviceIRQListHead[IRQ];
	while (list != NULL) {

		Device *d = list->ThisDevice;

		/* Just flag the device */
		d->IRQInterrupt = TRUE;

		/* Move to next device */
		list = list->NextDevice;


		/***** Should this be a semaphore to signal the device?? *****/
	}
}


void irq_InitIRQList( void ) {
	/* Sets up the table that contains the start of each list for the 16 IRQs */
	int i = 0;
	for (i=0; i<16; i++) {
		DeviceIRQListHead[i] = NULL;
	}
}


void irq_FreeIRQList( void ) {
	/* Frees all the lists */
	int i = 0;
	for (i=0; i<16; i++) {
		DeviceList *list = DeviceIRQListHead[i];
		DeviceList *next = NULL;
		while (list != NULL) {
			next = list->NextDevice;
			kfree(list);
			list = next;
		}
	}
}


void irq_RegisterIRQForDevice(Device *d, int IRQ) {
	if ((IRQ < 0) || (IRQ > 15)) {
		/* Bad IRQ number. Bail */
		return;
	}

	/* Create a new entry */
	DeviceList *newdevicelist = (DeviceList *) kmalloc(sizeof(DeviceList));
	newdevicelist->ThisDevice = d;
	newdevicelist->NextDevice = NULL;

	DeviceList *list = DeviceIRQListHead[IRQ];

	if (list == NULL) {
		/* No devices in the list yet */
		DeviceIRQListHead[IRQ] = newdevicelist;

	} else {
		/* Find the end of the list for this IRQ */
		DeviceList *list = DeviceIRQListHead[IRQ];
		DeviceList *last = NULL;
		while (list != NULL) {
			last = list;
			list = list->NextDevice;
		}

		if (last == NULL) {
			/* No entries. Should not be here. Recovering.... */
			DeviceIRQListHead[IRQ] = newdevicelist;
		} else {
			/* last now points at the last entry in the list. Add the entry */
			last->NextDevice = newdevicelist;
		}
	}
}


void irq_UnregisterIRQForDevice(Device *d, int IRQ) {
	if ((IRQ < 0) || (IRQ > 15)) {
		/* Bad IRQ number. Bail */
		return;
	}

	DeviceList *list = DeviceIRQListHead[IRQ];
	DeviceList *prev = NULL;
	DeviceList *next = NULL;

	/* Look at all the entries and remove all that refer to this device */
	while (list != NULL) {
		next = list->NextDevice;

		if (list->ThisDevice == d) {
			/* Found one. Remove from list. Prev doesn't move. */

			if (prev == NULL) {
				/* First in list */
				DeviceIRQListHead[IRQ]->NextDevice = next;
			} else {
				/* Not first in list */
				prev->NextDevice = next;
			}

			kfree(list);
		}

		list = next;	/* Next item */

	}
}

