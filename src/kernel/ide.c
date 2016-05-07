/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			ide.h
 * Description:		IDE hard disk driver
 * Author:			James Smith
 * Created:			26-May-2007
 * Last Modified:	26-May-2007
 *
 */
 
#include "kernel.h"
#include "ide.h"

WORD global_IDE1_BaseAddr = 0x1F0;

// Controller 1 at 0x1F0
// Controller 2 at 0x???

// Defined from controller base port
#define PORT_IDE_DATA          0x00
#define PORT_IDE_ERROR         0x01
#define PORT_IDE_SECTORS       0x02
#define PORT_IDE_SECTOR        0x03
#define PORT_IDE_CYLINDER_LOW  0x04
#define PORT_IDE_CYLINDER_HIGH 0x05
#define PORT_IDE_DRIVEHEAD     0x06
#define PORT_IDE_STATUS        0x07  // (Read)
#define PORT_IDE_COMMAND       0x07  // (Write)

// IDE Commands
#define IDE_CMD_RECALIBRATE    0x10  (bottom 4 bits = rate)
#define IDE_CMD_READSECTOR     0x20
#define IDE_CMD_WRITESECTOR    0x30
#define IDE_CMD_VERIFY         0x40  (also Scan ID)
#define IDE_CMD_FORMAT         0x50
#define IDE_CMD_SEEK           0x70  (bottom 4 bits = rate)
#define IDE_CMD_IDENTIFY       0xEC



int ide_InitDevice( void ) {
	// Do we need to do anything here?

	con_StreamWriteString( "[DEV] IDE Hard Disk Port=1F0\n" );

	return 0;
}


void ide_IRQHandler( void ) {
	con_StreamWriteString( "IDE Interrupt\n" );
}


void ide_ReadSector( BYTE drive, BYTE head, WORD cylinder, BYTE sector,
					BYTE *buffer ) {

	WORD base = global_IDE1_BaseAddr;

	// This code is CHS. How to we do this in LBA48?

	OUTPORT_BYTE( base + PORT_IDE_SECTORS, 1 );	// Just 1 sector
	OUTPORT_BYTE( base + PORT_IDE_SECTOR, sector );
	OUTPORT_BYTE( base + PORT_IDE_CYLINDER_LOW, cylinder & 0xFF );
	OUTPORT_BYTE( base + PORT_IDE_CYLINDER_HIGH, (cylinder >> 8) & 0xFF );
	OUTPORT_BYTE( base + PORT_IDE_DRIVEHEAD, 
		(head & 0x0F) + ((drive & 0x01) << 4) + 0xA0 );
	OUTPORT_BYTE( base + PORT_IDE_COMMAND, IDE_CMD_READSECTOR );

	// Wait for the data to be read
	BYTE status = 0x80;
	while (status & 0x80) {
		status = INPORT_BYTE( base + PORT_IDE_STATUS );
	}

	// Read the data
	int i = 0;
	WORD data = 0;
	for (i=0; i<512; i+=2) {
		data = INPORT_WORD( base + PORT_IDE_DATA );
		buffer[i] = data & 0xFF;
		buffer[i+1] = (data >> 8) & 0xFF;
	}
}


void ide_Identify( BYTE drive, BYTE *buffer ) {

}

