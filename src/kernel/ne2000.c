/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			ne2000.c
 * Description:		NE2000 network driver
 * Author:			James Smith
 * Created:			20-Apr-2007
 * Last Modified:	10-May-2007
 *
 */
 
#include "kernel.h"
#include "ne2000.h"

// NIC at 0x300, IRQ9
#define NE2K_BASE  0x300

DWORD global_NE2000Addr = NE2K_BASE;
static BYTE LocalMAC[6];


// PAGE 0, READ
#define NE2K_COMMAND 0x00	// Also write
#define NE2K_CLDA0   0x01
#define NE2K_BNDRY   0x03   // Also write
#define NE2K_ISR     0x07   // Also write
// PAGE 0, WRITE
#define NE2K_PSTART  0x01
#define NE2K_PSTOP   0x02
#define NE2K_TPSR    0x04
#define NE2K_TBCR0   0x05
#define NE2K_TBCR1   0x06
#define NE2K_RSAR0   0x08
#define NE2K_RSAR1   0x09
#define NE2K_RBCR0   0x0A
#define NE2K_RBCR1   0x0B
#define NE2K_RCR     0x0C
#define NE2K_TCR     0x0D
#define NE2K_DCR     0x0E
#define NE2K_IMR     0x0F
#define NE2K_DATA    0x10
// PAGE 1, READ/WRITE
#define NE2K_PAR0    0x01
#define NE2K_PAR1    0x02
#define NE2K_PAR2    0x03
#define NE2K_PAR3    0x04
#define NE2K_PAR4    0x05
#define NE2K_PAR5    0x06
#define NE2K_CURR    0x07
#define NE2K_MAR0    0x08
#define NE2K_MAR1    0x09
#define NE2K_MAR2    0x0A
#define NE2K_MAR3    0x0B
#define NE2K_MAR4    0x0C
#define NE2K_MAR5    0x0D
#define NE2K_MAR6    0x0E
#define NE2K_MAR7    0x0F


#define CMD_STOP        0x01
#define CMD_START       0x02
#define CMD_TRANSMIT    0x04
#define CMD_DMAREAD     0x08
#define CMD_DMAWRITE    0x10
#define CMD_DMASEND     0x18
#define CMD_DMACOMPLETE 0x20
#define CMD_PAGE0       0x00
#define CMD_PAGE1       0x40
#define CMD_PAGE2       0x80
#define CMD_PAGE3       0xC0

// 16K SRAM on board 0x4000->0x7FFF
#define READ_BUFFER_START   0x40    // Start receive buffer ring at 0x4000
#define READ_BUFFER_END     0x5F    // End   receive buffer ring at 0x5F00
#define WRITE_BUFFER_START  0x60    // Start transmit buffer ring at 0x6000
#define WRITE_BUFFER_END    0x6F	// End   transmit buffer ring at 0x6F00


static BYTE global_PacketBuffer[256];
static WORD global_PacketLength = 0;


static void RegWrite( WORD offset, BYTE value ) {
	OUTPORT_BYTE( global_NE2000Addr + offset, value );
}

static BYTE RegRead( WORD offset ) {
	return INPORT_BYTE( global_NE2000Addr + offset );
}

static void GetDeviceMAC( void ) {
	// Reads the MAC address from the EEPROM and stores it in LocalMAC,
	//  and the Physical Address Register

	// Set "start" and "DMAcomplete", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );

	// Write a 1 to bit6 of ISR to set "remote dma complete"
	RegWrite( NE2K_ISR, 0x40 );

	// Set start addr and byte count
	RegWrite( NE2K_RSAR0, 0x00 );
	RegWrite( NE2K_RSAR1, 0x00 );
	RegWrite( NE2K_RBCR0, (BYTE) 12 );
	RegWrite( NE2K_RBCR1, (BYTE) 0 );

	// Set "start" and "DMAread", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMAREAD );

	// Read every other byte as each part of the MAC is stored in a WORD
	//  rather than a BYTE
	int i = 0;
	BYTE value = 0;
	for (i=0; i<12; i++) {
		value = RegRead( NE2K_DATA );
		if (i&0x1) {
			// Skip this one
		} else {
			// Get the data from the FIFO
			LocalMAC[i>>1] = value;
		}
	}
}


int net_InitDevice( void ) {

	// Set "stop" and "dmacomplete"
	RegWrite( NE2K_COMMAND, CMD_STOP | CMD_DMACOMPLETE | CMD_PAGE0 );

	// Initialise DCR, WORD-wide single 32bit DMA
	RegWrite( NE2K_DCR, 0x0C );

	// Clear remote byte count registers
	RegWrite( NE2K_RBCR0, 0x00 );
	RegWrite( NE2K_RBCR1, 0x00 );

	// Initialise RCR, Accept broadcast and runt packets
	RegWrite( NE2K_RCR, 0x06 );

	// Set LOOPBACK mode 1
	RegWrite( NE2K_TCR, 0x02 );

	// Initialise receive buffer ring
	RegWrite( NE2K_BNDRY, READ_BUFFER_START );
	RegWrite( NE2K_PSTART, READ_BUFFER_START );
	RegWrite( NE2K_PSTOP, READ_BUFFER_END );

	// Clear interrupt status register
	RegWrite( NE2K_ISR, 0xFF );

	// Initialise interrupt mask register
	// Tx/Rx and Tx/Rx errors interrupts enabled
	RegWrite( NE2K_IMR, 0x0F );

	GetDeviceMAC();

	RegWrite( NE2K_COMMAND, CMD_STOP | CMD_DMACOMPLETE | CMD_PAGE1 );

	// Initialise physical/multicast address registers
	// Shouldn't this be already set by the card?
	int i = 0;
	for (i=0; i<6; i++) {
		RegWrite( NE2K_PAR0 + i, LocalMAC[i] );
	}
	RegWrite( NE2K_MAR0, 0xFF );
	RegWrite( NE2K_MAR1, 0xFF );
	RegWrite( NE2K_MAR2, 0xFF );
	RegWrite( NE2K_MAR3, 0xFF );
	RegWrite( NE2K_MAR4, 0xFF );
	RegWrite( NE2K_MAR5, 0xFF );
	RegWrite( NE2K_MAR6, 0xFF );
	RegWrite( NE2K_MAR7, 0xFF );

	// Initialise current pointer
	RegWrite( NE2K_CURR, READ_BUFFER_START );  // Set to PSTART

	// Put NIC in start mode
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE | CMD_PAGE0 );

	// Initialise the transmit configuration for real settings
	RegWrite( NE2K_TCR, 0x00 );

	con_StreamWriteString( "[DEV] NE2000 Base=" );
	con_StreamWriteHexDWORD( global_NE2000Addr );
	con_StreamWriteString( " MAC=" );
	for (i=0; i<6; i++) {
		if (i>0) {
			con_StreamWriteChar( ':' );
		}
		con_StreamWriteHexBYTE( LocalMAC[i] );
	}
	con_StreamWriteChar( '\n' );

	return 0;
}

void net_IRQHandler( void ) {

	// Read the ISR and check status
	BYTE isr = RegRead( NE2K_ISR );
	if (isr & 0x01) {
//		con_StreamWriteString( "PACKET RX\n" );

		global_PacketLength = 1;
		while (global_PacketLength != 0) {
			net_ReadPacket( global_PacketBuffer, &global_PacketLength );
			if (global_PacketLength != 0) {
				// TODO: Should put the packet in a queue to be picked
				//  up by the TCP/IP stack later.
				ip_ProcessPacket( global_PacketBuffer, global_PacketLength );
			}
		}
		RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );
		RegWrite( NE2K_ISR, 0x01 );
	}
	if (isr & 0x02) {
//		con_StreamWriteString( "PACKET TX\n" );
		RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );
		RegWrite( NE2K_ISR, 0x02 );
	}
	if (isr & 0x04) {
		con_StreamWriteString( "RX ERROR\n" );
		RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );
		RegWrite( NE2K_ISR, 0x04 );
	}
	if (isr & 0x08) {
		con_StreamWriteString( "TX ERROR\n" );
		RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );
		RegWrite( NE2K_ISR, 0x08 );
	}
}


void net_ReadPacket( BYTE *data, WORD *length ) {

	// Get the current BOUNDARY pointer (next page to read)
	// Set "start" and "DMAcomplete", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );
	BYTE Boundary = RegRead( NE2K_BNDRY );
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE | CMD_PAGE1 );
	BYTE Current = RegRead( NE2K_CURR );

/*
	con_StreamWriteString( "NETREAD: Boundary=" );
	con_StreamWriteHexBYTE( Boundary );
	con_StreamWriteString( " Current=" );
	con_StreamWriteHexBYTE( Current );
	con_StreamWriteString( "\n" );
*/
	if (Boundary == Current) {
//		con_StreamWriteString( "No data to read\n " );
		// No data to read
		*length = 0;
		return;
	}

	// Alloc a buffer for the page
	BYTE tmpbuf[256];

	// Set "start" and "DMAcomplete", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );

	// Read the page data
	// Write a 1 to bit6 of ISR to set "remote dma complete"
	RegWrite( NE2K_ISR, 0x40 );

	// Write a non-zero value to RBCR0
	RegWrite( NE2K_RBCR0, 0x01 );

	// Need to set remote DMA read first
	// Set "start" and "DMAread", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMAREAD );

	// Set start addr and byte count
	RegWrite( NE2K_RSAR0, 0x00 );
	RegWrite( NE2K_RSAR1, Boundary );
	RegWrite( NE2K_RBCR0, (BYTE) 0x00 );
	RegWrite( NE2K_RBCR1, (BYTE) 0x01 );  // 1 page of 256 bytes

	// Set "start" and "DMAread", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMAREAD );

	int i = 0;
	for (i=0; i<256; i++) {
		tmpbuf[i] = RegRead( NE2K_DATA );
	}

	// Copy from the page to the destination buffer
	*length = tmpbuf[2] + (tmpbuf[3] << 8);
	memcpy( &tmpbuf[4], data, *length );

	// Increment the boundary pointer
	//  NB: Need to check PSTOP....
	Boundary++;
	if (Boundary >= READ_BUFFER_END) {
		Boundary = READ_BUFFER_START;
	}
	RegWrite( NE2K_BNDRY, Boundary );

}


void net_ReadData( BYTE *data, WORD *length ) {

	// Set "start" and "DMAcomplete", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );

	// Write a 1 to bit6 of ISR to set "remote dma complete"
	RegWrite( NE2K_ISR, 0x40 );

	*length = 256;

	// Write a non-zero value to RBCR0
	RegWrite( NE2K_RBCR0, 0x01 );

	// Need to set remote DMA read first
	// Set "start" and "DMAread", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMAREAD );
	// Set start addr and byte count
	RegWrite( NE2K_RSAR0, 0x00 );
	RegWrite( NE2K_RSAR1, READ_BUFFER_START );
	RegWrite( NE2K_RBCR0, (BYTE) (*length & 0xFF) );
	RegWrite( NE2K_RBCR1, (BYTE) (*length >> 8) );

	// Set "start" and "DMAread", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMAREAD );

	int i = 0;
	for (i=0; i<*length; i++) {
		data[i] = RegRead( NE2K_DATA );
	}

}

static void WriteTransmitBuffer( WORD addr, BYTE *data, WORD length ) {

	// Set "start" and "DMAcomplete", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE );

	// Write a 1 to bit6 of ISR to set "remote dma complete"
	RegWrite( NE2K_ISR, 0x40 );

	// Write a non-zero value to RBCR0
	RegWrite( NE2K_RBCR0, 0x01 );

	// Need to set remote DMA read first
	// Set "start" and "DMAread", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMAREAD );

	// Set start addr and byte count
	RegWrite( NE2K_RSAR0, (BYTE) (addr & 0xFF) );
	RegWrite( NE2K_RSAR1, (BYTE) (addr >> 8) );
	RegWrite( NE2K_RBCR0, (BYTE) (length & 0xFF) );
	RegWrite( NE2K_RBCR1, (BYTE) (length >> 8) );

	// Set the remote DMA command
	// Set "start" and "DMAwrite", PAGE 0
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMAWRITE );

	// Write the bytes to the data port
	int i = 0;
	for (i=0; i<length; i++) {
		RegWrite( NE2K_DATA, data[i] );
	}

	// Wait for the DMA to signal completion
	while (!(RegRead( NE2K_ISR ) & 0x40));

}

//
// destMAC = 6 byte array of destination MAC address
// data    = length byte array of data to be transmitted
// length  = 1-1500 packet length
//
void net_SendPacket( BYTE *destMAC, BYTE *data, WORD length,
					WORD frameType ) {

	// Why does this appear to be the same location as the 
	//  READ BUFFER START?????

	WORD CurrentAddr = (WORD)(WRITE_BUFFER_START << 8) & 0xFFFF;
	WORD ActualLength = 0;

	// 0x0806 = ARP,  0x0800 = IP
	BYTE FrameType[2];
	FrameType[0] = (frameType >> 8) & 0xFF;	
	FrameType[1] = (frameType & 0xFF);

	// Destination MAC
	WriteTransmitBuffer( CurrentAddr, destMAC, 6 );
	CurrentAddr += 6;

	// Source MAC
	WriteTransmitBuffer( CurrentAddr, LocalMAC, 6 );
	CurrentAddr += 6;

	// Frame Type
	WriteTransmitBuffer( CurrentAddr, FrameType, 2 );
	CurrentAddr += 2;

	// Packet Data
	//  Do we need to split packets larger than 256 bytes?
	WriteTransmitBuffer( CurrentAddr, data, length );
	CurrentAddr += length;

	// Pad if necessary  (64 bytes total => 46 bytes data minimum)
	if ((length < 46) && (1)) {
		BYTE pad[46];
		int i = 0;
		for (i=0; i<46; i++) {
			pad[i] = 0;
		}
		WriteTransmitBuffer( CurrentAddr, pad, 46 - length );
		ActualLength = 64;
	} else {
		ActualLength = length + 18;
	}

	// Packet has now been copied to the data buffer, now send it
	
	// Set transmit page start and transmit byte count
	RegWrite( NE2K_TPSR, WRITE_BUFFER_START );
	RegWrite( NE2K_TBCR0, (BYTE) (ActualLength & 0xFF) );
	RegWrite( NE2K_TBCR1, (BYTE) (ActualLength >> 8) );

	// Set TXP bit to start the transmission
	// Set "start", "DMAcomplete" and "transmit"
	RegWrite( NE2K_COMMAND, CMD_START | CMD_DMACOMPLETE | CMD_TRANSMIT );

}

void net_GetLocalMAC( BYTE *localMAC ) {
	// Write the local MAC address to the supplied buffer
	int i = 0;

	for (i=0; i<6; i++) {
		localMAC[i] = LocalMAC[i];
	}
}
