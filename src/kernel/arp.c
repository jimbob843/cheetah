/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			arp.c
 * Description:		Internet Protocol
 * Author:			James Smith
 * Created:			30-Apr-2007
 * Last Modified:	04-May-2007
 *
 */
 
#include "kernel.h"
#include "ne2000.h"
#include "arp.h"

// TODO: There are multiple definitions of this!
static BYTE LocalMAC[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static BYTE BroadcastMAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static DWORD LocalIP = 0xC0A80509;  // 192.168.5.9

static ArpEntry *ArpTableHead = NULL;
static ArpEntry *ArpTableTail = NULL;


void arp_GenerateRequest( DWORD targetIP ) {

	// Build an ARP request and give to the network driver
	BYTE buffer[28];
	int i = 0;

	// Get a copy of the local MAC address
	net_GetLocalMAC( LocalMAC );

	buffer[0] = 0x00;  // Hardware type 0x0001 = Ethernet
	buffer[1] = 0x01;
	buffer[2] = 0x08;  // Protocol type 0x0800 = IP
	buffer[3] = 0x00;
	buffer[4] = 0x06;  // HLEN - MAC addr length
	buffer[5] = 0x04;  // PLEN - Protocol addr length
	buffer[6] = 0x00;  // Operation 0x0001 = ARP request
	buffer[7] = 0x01;

	// Sender MAC
	for (i=0; i<6; i++) {
		buffer[8+i] = LocalMAC[i];
	}

	// Sender IP
	buffer[14] = (LocalIP >> 24) & 0xFF;
	buffer[15] = (LocalIP >> 16) & 0xFF;
	buffer[16] = (LocalIP >> 8) & 0xFF;
	buffer[17] = (LocalIP) & 0xFF;

	// Target MAC
	for (i=0; i<6; i++) {
		buffer[18+i] = 0x00;
	}

	// Target IP
	buffer[24] = (targetIP >> 24) & 0xFF;
	buffer[25] = (targetIP >> 16) & 0xFF;
	buffer[26] = (targetIP >> 8) & 0xFF;
	buffer[27] = (targetIP) & 0xFF;

	// Transmit
	net_SendPacket( BroadcastMAC, buffer, 28, FRAMETYPE_ARP );
}


void arp_GenerateReply( BYTE *request, WORD length ) {

	DWORD RequestedIP = (request[24] << 24) + (request[25] << 16)
		              + (request[26] <<  8) + (request[27]);

	// First need to check that this is a request about our IP
	if (RequestedIP == LocalIP) {
			// Yes. This is a request to us.
	} else {
		// Nope. This is for someone else.
//		con_StreamWriteString( "ARP Request for " );
//		con_StreamWriteHexDWORD( RequestedIP );
//		con_StreamWriteString( ". Ignored.\n" );
		return;
	}

	// Build an ARP reply and give to the network driver
	BYTE buffer[28];
	int i = 0;

	// Get a copy of the local MAC address
	net_GetLocalMAC( LocalMAC );

	buffer[0] = 0x00;  // Hardware type 0x0001 = Ethernet
	buffer[1] = 0x01;
	buffer[2] = 0x08;  // Protocol type 0x0800 = IP
	buffer[3] = 0x00;
	buffer[4] = 0x06;  // HLEN - MAC addr length
	buffer[5] = 0x04;  // PLEN - Protocol addr length
	buffer[6] = 0x00;  // Operation 0x0002 = ARP reply
	buffer[7] = 0x02;

	// Sender MAC
	for (i=0; i<6; i++) {
		buffer[8+i] = LocalMAC[i];
	}

	// Sender IP
	buffer[14] = (LocalIP >> 24) & 0xFF;
	buffer[15] = (LocalIP >> 16) & 0xFF;
	buffer[16] = (LocalIP >> 8) & 0xFF;
	buffer[17] = (LocalIP) & 0xFF;

	// Target MAC
	for (i=0; i<6; i++) {
		// Copy from ARP request
		buffer[18+i] = request[8+i];
	}

	// Target IP, copy from ARP request
	buffer[24] = request[14];
	buffer[25] = request[15];
	buffer[26] = request[16];
	buffer[27] = request[17];


	// REPORT
/*
	con_StreamWriteString( "ARP Reply to IP=" );
	for (i=0; i<4; i++) {
		if (i>0) {
			con_StreamWriteChar( '.' );
		}
		con_StreamWriteHexBYTE( buffer[24+i] );
	}
	con_StreamWriteString( " MAC=" );
	for (i=0; i<6; i++) {
		if (i>0) {
			con_StreamWriteChar( ':' );
		}
		con_StreamWriteHexBYTE( buffer[18+i] );
	}
	con_StreamWriteChar( '\n' );
*/

	// Transmit back to sender's MAC
	net_SendPacket( &request[8], buffer, 28, FRAMETYPE_ARP );

	// TODO: Also store this new information in the ARP table?

}

void arp_HandleReply( BYTE *buffer, WORD length ) {

	// This is a reply to us from a request that we made.
	// Take the data and apply it to the ARP table.

	DWORD ip = (buffer[14] << 24) + (buffer[15] << 16)
		     + (buffer[16] <<  8) + (buffer[17]);

	// Add this one to the ARP table
	arp_AddEntry( ip, &buffer[8] );

/*
	// Also report to the console.
	int i = 0;

	con_StreamWriteString( "ARP Reply from IP=" );
	for (i=0; i<4; i++) {
		if (i>0) {
			con_StreamWriteChar( '.' );
		}
		con_StreamWriteHexBYTE( buffer[14+i] );
	}
	con_StreamWriteString( " MAC=" );
	for (i=0; i<6; i++) {
		if (i>0) {
			con_StreamWriteChar( ':' );
		}
		con_StreamWriteHexBYTE( buffer[8+i] );
	}
	con_StreamWriteChar( '\n' );
*/
}


void arp_PacketHandler( BYTE *buffer, WORD length ) {

	// Extract the ARP operation code from the packet
	WORD operation = (buffer[6] >> 8) + buffer[7];

	switch (operation) {
		case 0x0001:
			// ARP Request from someone else
			arp_GenerateReply( buffer, length );
			break;
		case 0x0002:
			// ARP Reply
			arp_HandleReply( buffer, length );
			break;
		default:
			// RARP or unknown. Discard.
			con_StreamWriteString( "Unknown ARP operation " );
			con_StreamWriteHexDWORD( operation );
			con_StreamWriteString( ". Discarded.\n" );
			break;
	}
}


void arp_AddEntry( DWORD ip, BYTE *mac ) {

	// First check to see if this entry exist already
	BYTE *hasmac = arp_LookupMAC( ip );

	if (hasmac != NULL) {
		// This ip already exists in the table.
		// TODO: Check to see if the MAC has changed?
		return;
	}
/*
	con_StreamWriteString( "Adding ARP entry for " );
	con_StreamWriteHexDWORD( ip );
	con_StreamWriteString( " having MAC=" );
	if (mac == NULL) {
		con_StreamWriteString( "NONE" );
	} else {
		int i = 0;
		for (i=0; i<6; i++) {
			if (i>0) {
				con_StreamWriteChar( ':' );
			}
			con_StreamWriteHexBYTE( mac[i] );
		}
	}
	con_StreamWriteString( "\n" );
*/

	ArpEntry *x = (ArpEntry*)kmalloc(sizeof(ArpEntry));

	// Populate the new entry
	x->IPAddress = ip;
	memcpy( mac, &(x->MACAddress), 6 );
	x->NextEntry = NULL;

	// Add to the entry table
	if (ArpTableTail == NULL) {
		// This is the first entry
		ArpTableHead = x;
		ArpTableTail = x;
	} else {
		// Add the new one to the end
		ArpTableTail->NextEntry = x;
		ArpTableTail = x;
	}	
}


BYTE *arp_LookupMAC( DWORD ip ) {

	// Loop through all the entries in the arp table for the IP
	// Return a pointer to the MAC, or NULL if it doesn't exist

	ArpEntry *current = ArpTableHead;
	BYTE *mac = NULL;

	while ((mac == NULL) && (current != NULL)) {
		if (current->IPAddress == ip) {
			// Found it!
			mac = (BYTE*) &(current->MACAddress);
		}
		// Move to the next one
		current = current->NextEntry;
	}
/*
	con_StreamWriteString( "ARP Lookup for " );
	con_StreamWriteHexDWORD( ip );
	con_StreamWriteString( " gives MAC=" );
	if (mac == NULL) {
		con_StreamWriteString( "NONE" );
	} else {
		int i = 0;
		for (i=0; i<6; i++) {
			if (i>0) {
				con_StreamWriteChar( ':' );
			}
			con_StreamWriteHexBYTE( mac[i] );
		}
	}
	con_StreamWriteString( "\n" );
*/
	return mac;
}


void arp_DumpARPTable( void ) {

	ArpEntry *current = ArpTableHead;
	int i = 0;

	con_StreamWriteString( "=== ARP TABLE ==\n" );
	con_StreamWriteString( "IP        MAC\n" );
	if (current == NULL) {
		con_StreamWriteString( "No Entries\n" );
	}

	while (current != NULL) {
		con_StreamWriteHexDWORD( current->IPAddress );
		con_StreamWriteString( "  " );
		for (i=0; i<6; i++) {
			if (i>0) {
				con_StreamWriteChar( ':' );
			}
			con_StreamWriteHexBYTE( current->MACAddress[i] );
		}
		current = current->NextEntry;
	}
}
