/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			floppy.c
 * Description:		floppy disk driver
 * Author:			James Smith
 * Created:			05-Sep-2006
 * Last Modified:	30-May-2007
 *
 */
 
#include "kernel.h"
#include "floppy.h"

// Controller 1 at 0x3F0
// Controller 2 at 0x370

#define PORT_FLOPPY_SRA  0x3F0  // Status Register A (R)
#define PORT_FLOPPY_SRB  0x3F1  // Status Register B (R)
#define PORT_FLOPPY_DOR  0x3F2  // Digital Output Register (R/W)
#define PORT_FLOPPY_TDR  0x3F3  // Tape Drive Register (R/W)
#define PORT_FLOPPY_MSR  0x3F4  // Main Status Register (R)
#define PORT_FLOPPY_DSR  0x3F4  // Data Rate Select Register (W)
#define PORT_FLOPPY_FIFO 0x3F5  // Data FIFO (R/W)
#define PORT_FLOPPY_DIR  0x3F7  // Digital Input Register (R)
#define PORT_FLOPPY_CCR  0x3F7  // Configuration Control Register (W)

static BOOL interrupt_ack = FALSE;
static BYTE *CurrentFAT = NULL;


static void WaitFloppyInterrupt( void ) {
	// Wait here until IRQ6 has fired
	while (interrupt_ack == FALSE) {
		// Wait?
	}

	// Reset for the next one
	interrupt_ack = FALSE;
}

static void WaitDriveReady( void ) {
	// Checks the RQM bit (bit 7) in the Main Status Register (MSR)
	// When 1 indicates that drive is ready
	// Also checks the DIO=1 for a write
	BYTE msr = 0;

	do {
		msr = INPORT_BYTE( PORT_FLOPPY_MSR );
	} while (!(msr & 0x80) || (msr & 0x40));
}

static void WaitDriveReadyRead( void ) {
	// Checks the RQM bit (bit 7) in the Main Status Register (MSR)
	// When 1 indicates that drive is ready
	// Also checks the DIO=0 for a read
	BYTE msr = 0;

	do {
		msr = INPORT_BYTE( PORT_FLOPPY_MSR );
	} while (!(msr & 0x80) || !(msr & 0x40));
}

static void SendFloppyByte( BYTE value ) {
	WaitDriveReady();
	OUTPORT_BYTE( PORT_FLOPPY_FIFO, value );
}

static BYTE ReadFloppyByte( void ) {
	WaitDriveReadyRead();
	return INPORT_BYTE( PORT_FLOPPY_FIFO );
}

static void SenseInterruptStatus( void ) {
	// Issues "Sense Interrupt Status" (0x08) to retreive
	//  information after an interrupt. This tells the
	//  controller that we acknowledge the interrupt.

	WaitDriveReady();
	OUTPORT_BYTE( PORT_FLOPPY_FIFO, 0x08 );		// Command

	WaitDriveReadyRead();
	BYTE ST0 = INPORT_BYTE( PORT_FLOPPY_FIFO );

	if (ST0 != 0x80) {
		WaitDriveReadyRead();
		BYTE PCN = INPORT_BYTE( PORT_FLOPPY_FIFO );
	}
}

static void FloppyRecalibrate( BYTE drive ) {
	// Recalibrate command (0x07)
	// Moves the head to track 0. 
	WaitDriveReady();
	OUTPORT_BYTE( PORT_FLOPPY_FIFO, 0x07 );		// Command
	WaitDriveReady();
	OUTPORT_BYTE( PORT_FLOPPY_FIFO, drive );	// Drive number
	
	// Must acknowledge by waiting for interrupt and then
	//  issuing a "Sense Interrupt Status"
	WaitFloppyInterrupt();
	SenseInterruptStatus();
}

void flp_SeekTrack( BYTE drive, BYTE head, BYTE cylinder ) {
	// Seek Track command (0x0F)
	SendFloppyByte( 0x0F );
	SendFloppyByte( ((head & 0x01) << 2) | (drive & 0x03) );
	SendFloppyByte( cylinder );

	// Check ST0, bit SE=1 for completion?

	SenseInterruptStatus();

	// Verify head location?

}

static void init_dma( void ) {
	OUTPORT_BYTE( 0x0A, 0x06 );  // Mask Channel 2
	OUTPORT_BYTE( 0xD8, 0xFF );  // Reset the master flip-flop
	OUTPORT_BYTE( 0x04, 0x00 );  // Buffer LSB
	OUTPORT_BYTE( 0x04, 0x05 );  // Buffer MSB  (0x0500)
	OUTPORT_BYTE( 0xD8, 0xFF );  // Reset the master flip-flop (again)
	OUTPORT_BYTE( 0x05, 0xFF );  // Byte count LSB
	OUTPORT_BYTE( 0x05, 0x01 );  // Byte count MSB  (0x0200)-1
	OUTPORT_BYTE( 0x81, 0x00 );  // Page register 0 (for buffer address)
	OUTPORT_BYTE( 0x0A, 0x02 );  // Unmask Channel 2
}


static void start_dma_read( void ) {

	// Clear the DMA buffer
	int i = 0;
	BYTE *dma_buffer = (BYTE *) 0x0500;
	for (i=0; i<512; i++) {
		dma_buffer[i] = 0x00;
	}

	OUTPORT_BYTE( 0x0A, 0x06 );	// Mask Channel 2
	OUTPORT_BYTE( 0x0B, 0x46 ); // 10001010 - Write to memory (from device)
                                // single transfer, address increment, autoinit, read, channel2
	OUTPORT_BYTE( 0x0A, 0x02 );	// Unmask Channel 2
}


static void start_dma_write( void ) {
	// Clear the DMA buffer
	int i = 0;
	BYTE *dma_buffer = (BYTE *) 0x0500;
	for (i=0; i<512; i++) {
		dma_buffer[i] = 0x00;
	}

	OUTPORT_BYTE( 0x0A, 0x06 );	// Mask Channel 2
	OUTPORT_BYTE( 0x0B, 0x4A ); // 01001010 - Write to device (from memory)
                                // single transfer, address increment, autoinit, write, channel2
	OUTPORT_BYTE( 0x0A, 0x02 );	// Unmask Channel 2
}


void flp_ReadSector( BYTE drive, BYTE head, BYTE cylinder, BYTE sector, 
						BYTE *buffer ) {

	OUTPORT_BYTE( PORT_FLOPPY_DOR, 0x1C );  // Drive on

	flp_SeekTrack( drive, head, cylinder );

	// Start DMA
	init_dma();	// Shouldn't have to do this every time, but it crashes otherwise
	start_dma_read();

	// Delay for head settle time
	int j = 0;
	for (j=0; j<10000; j++) ;

	// Send command (0x06)  - Needs MFM bit?
	SendFloppyByte( 0x66 );    // Command
	SendFloppyByte( (head&0x01)<<2 | (drive & 0x03) );
	SendFloppyByte( cylinder );
	SendFloppyByte( head );
	SendFloppyByte( sector );
	SendFloppyByte( 0x02 );    // 0x02 = 512 bytes
	SendFloppyByte( 18 );      // 18 sectors per cylinder
	SendFloppyByte( 0x1B );	   // GPL from datasheet
	SendFloppyByte( 0xFF );    // DTL no used

	WaitFloppyInterrupt();

	// Read result phase
	BYTE ST0 = ReadFloppyByte();
	BYTE ST1 = ReadFloppyByte();
	BYTE ST2 = ReadFloppyByte();
	BYTE rcylinder = ReadFloppyByte();
	BYTE rhead = ReadFloppyByte();
	BYTE rsector = ReadFloppyByte();
	BYTE rbytespersector = ReadFloppyByte();

	OUTPORT_BYTE( PORT_FLOPPY_DOR, 0x0C );  // Drive off

	// Copy DMA buffer to user buffer
	int i = 0;
	BYTE *dma_buffer = (BYTE *) 0x0500;
	for (i=0; i<512; i++) {
		buffer[i] = dma_buffer[i];
	}
}


int flp_InitDevice( void ) {

	int i = 0;

	// Need interrupt enabled
	ENABLE_IRQ( 6 );

	// Clear bit 2 to cause a RESET
	OUTPORT_BYTE( PORT_FLOPPY_DOR, 0x00 );

	// Clear bits 0-1 to select drive 0
	// Set bit 2 for running.
	// Set bit 3 to turn off DMA
	// Clear bits 4-7 to turn off all motors
//	OUTPORT_BYTE( PORT_FLOPPY_DOR, 0x1C );  // Set bit 2 and 3 for DMA and RESET off
	OUTPORT_BYTE( PORT_FLOPPY_DOR, 0x0C );  // Set bit 2 and 3 for DMA and RESET off

	// Program data rate via CCR (select default PRECOMP, 500Kbps)
	// The defaults are best for a 3+1/2" floppy.
	OUTPORT_BYTE( PORT_FLOPPY_CCR, 0x00 );

	WaitFloppyInterrupt();

	// Repeat 4 times to clear the status for all 4 drives
	for (i=0; i<4; i++) {	
		SenseInterruptStatus();
	}

	// We can send a Configure command here if necessary, but we're
	//  happy with the defaults for now

	// Specify command (0x03) - used to set drive delay values.
	// HUT - Head Unload Time
	// SRT - Step Rate Time
	// HLT - Head Load Time
	// ND bit - 1 => no DMA
	// These values are currently set to the largest delay times
	WaitDriveReady();
	OUTPORT_BYTE( PORT_FLOPPY_FIFO, 0x03 );		// Command
	WaitDriveReady();
	OUTPORT_BYTE( PORT_FLOPPY_FIFO, 0x85 );		// SRT 8 (MSB) + HUT 5 (LSB)
	WaitDriveReady();
	OUTPORT_BYTE( PORT_FLOPPY_FIFO, 0x1E );		// HLT 15 + ND bit=0

	// Move the head to track 0. Required after reset.
	// Needed for each drive, but only supporting one drive at the moment.
	FloppyRecalibrate( 0x00 );

	con_StreamWriteString( "[DEV] Floppy Port=3F0\n" );

	return 0;
}


void flp_IRQHandler( void ) {
	interrupt_ack = TRUE;
}



#if 0
int flp_LoadNextCluster( FILE *fp, BYTE *buffer ) {
	// Move to the next cluster, and load data
	int nextcluster = flp_GetNextCluster( fp->currentcluster );
	if (nextcluster >= 0x0FFF) {
		// no more clusters
		return -1;
	}
	int result = flp_LoadCluster( nextcluster, buffer );
	if (result == 0) {
		// Loaded ok
		fp->currentcluster = nextcluster;
		if (fp->buffer) {
			kfree( fp->buffer );
		}
		fp->buffer = buffer;
		fp->bufferposition = 0;
	} else {
		// Could not load cluster
		return -1;
	}
}


BYTE *flp_LoadFAT( void ) {
	// Loads the FAT into a new buffer
	// Need to read 9 sectors per FAT
	int i = 0;

	if (CurrentFAT != NULL) {
		kfree( CurrentFAT );
	}
	CurrentFAT = (BYTE *)kmalloc( 9 * 512 );

	for (sector = 0; sector < 9; sector++) {
		flp_ReadSector( 0, 0, 0, 2 + sector );
	}
}

BYTE *flp_LoadBPB( void ) {
	// Loads the BPB into a new buffer
}

BYTE *flp_LoadDirectory( void ) {
	// Loads the disk directory into a new buffer
}


BYTE *flp_FindDirectoryEntry( char *filename ) {
	
}

void flp_LoadCluster( int clusternum, BYTE *buffer ) {
	// Loads a sector from disk into the buffer
	BYTE head = 0;
	BYTE cylinder = 0;
	BYTE sector = 0;

	// Need to do LBA to CHS conversion, and then read sector
	LBAtoCHS( clusternum, &head, &cylinder, &sector );
	flp_ReadSector( 0, head, cylinder, sector, buffer );
}

int flp_GetNextCluster( int clusternum ) {
	// Looks at the FAT, and returns the next clusternum

	if (clusternum >= 0x0FFF) {
		// we're already at the end of the chain
		return 0x0FFF;
	}

	if (CurrentFAT == NULL) {
		flp_LoadFAT();
	}

	// Calculate position in FAT for this cluster
	int pos = clusternum * 3 / 2;		// 3/2 bytes per entry.
	WORD data = CurrentFAT[pos];
	if (clusternum & 0x01) {
		// clusternum is odd
		return (data >> 4) & 0xFFF;		// Shift the data into the right place
	} else {
		// clusternum is even
		return data & 0xFFF;			// Just mask the bits we want
	}
}

int flp_GetNthCluster( int startclusternum, int numclusters ) {
	// Moves forward N clusters in the FAT
	int currentcluster = startclusternum;
	while ((numclusters > 0) || (currentcluster >= 0x0FFF)) {
		currentcluster = flp_GetNextCluster( currentcluster );
	}
	return currentcluster;
}

#endif
