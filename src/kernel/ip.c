/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			ip.c
 * Description:		Internet Protocol
 * Author:			James Smith
 * Created:			29-Apr-2007
 * Last Modified:	10-May-2007
 *
 */
 
#include "kernel.h"
#include "ne2000.h"
#include "ip.h"
#include "arp.h"
#include "udp.h"

static BYTE LocalMAC[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static BYTE BroadcastMAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static DWORD LocalIP = 0xC0A80509;  // 192.168.5.9



void ip_PacketHandler( BYTE *buffer, WORD length ) {

	BYTE protocol = buffer[9];  // TODO: Find correct offset and IP
	DWORD sourceIP = (buffer[12] << 24) + (buffer[13] << 16)
		           + (buffer[14] <<  8) + (buffer[15]);

	// Bottom nibble of byte 0 contain number of DWORDs in header
	BYTE HeaderLength = (buffer[0] & 0x0F) << 2;
	WORD DataLength = (buffer[2] << 8) + buffer[3];
	BYTE *PacketData = &buffer[HeaderLength];
	WORD PacketDataLength = DataLength - HeaderLength;

	if (length < (HeaderLength + PacketDataLength)) {
		// Length field in the header is bigger than the data read
		con_StreamWriteString( "WARNING! Bad data length in IP header.\n" );
		return;
	}
/*
	con_StreamWriteString( "IP from " );
	con_StreamWriteHexDWORD( sourceIP );
	con_StreamWriteString( " Protocol=" );
	con_StreamWriteHexBYTE( protocol );
	con_StreamWriteString( " HeaderLength=" );
	con_StreamWriteHexBYTE( HeaderLength );
	con_StreamWriteString( " DataLength=" );
	con_StreamWriteHexDWORD( PacketDataLength );
	con_StreamWriteString( "\n" );
*/
	// TODO: Check destination and forward packets that aren't ours?

	switch (protocol) {
		case PROTOCOL_ICMP:
			icmp_PacketHandler( PacketData, PacketDataLength, sourceIP );
			break;
		case PROTOCOL_UDP:
			udp_PacketHandler( PacketData, PacketDataLength, sourceIP );
			break;
		case PROTOCOL_TCP:
			break;
		default:
			// unknown
			break;
	}
}


void ip_ProcessPacket( BYTE *buffer, WORD length ) {

//	con_DumpMemoryBlock( buffer, 2 );

	if (length == 0) {
		con_StreamWriteString( "WARNING: Zero length packet!\n" );
		return;
	}

	// Work out what the packet is and deal with it
	WORD FrameType = (buffer[12] << 8) + buffer[13];
	BYTE *PacketData = &buffer[14];
	WORD PacketDataLength = length - 14;

	switch (FrameType) {
		case 0x0800:
			ip_PacketHandler( PacketData, PacketDataLength );
			break;
		case 0x0806:
			arp_PacketHandler( PacketData, PacketDataLength );
			break;
		default:
			// Unknown packet type. Discard.
			con_StreamWriteString( "Unknown packet. Discarded. FrameType=" );
			con_StreamWriteHexDWORD( FrameType );
			con_StreamWriteChar( '\n' );
			break;
	}

	sch_ProcessNotify( 0x0002 );
}

BYTE *ip_GetMACFromIP( DWORD targetIP ) {

	// Look up the IP in the ARP table
	BYTE *mac = arp_LookupMAC( targetIP );
	if (mac == NULL) {
		// Couldn't find it. Send an ARP request
		arp_GenerateRequest( targetIP );

//		while (mac == NULL) {
//			// Wait for the reply....
//			con_StreamWriteString( "Waiting for ARP reply\n" );
//			sch_ProcessWait( (DWORD)0x00000002 );
//			mac = arp_LookupMAC( targetIP );
//		}
	}

	return mac;
}


void ip_SendPacket( BYTE *buffer, WORD length, DWORD targetIP,
					BYTE protocol ) {

	BYTE *IPPacket = (BYTE*)kmalloc( length + 20 );
	WORD IPLength = 0;

	// TODO: Check subnet
	//       Use MAC of gateway if not this subnet

	// Translate the IP address into a MAC address
	BYTE *DestMAC = ip_GetMACFromIP( targetIP );

	if (DestMAC == NULL) {
		// Erk! No destination!
		con_StreamWriteString( "No destination MAC\n" );
		return;
	}

	IPPacket[0] = 0x45;		// IPv4 & 5xDWORD header
	IPPacket[1] = 0x0C;		// Service - Pool2 - Experimental
	// Need to check input params
	IPLength = length + 20;
	IPPacket[2] = (IPLength >> 8) & 0xFF;
	IPPacket[3] = (IPLength & 0xFF);

	IPPacket[4] = 0x00;		// Identification
	IPPacket[5] = 0x00;

	IPPacket[6] = 0x00;		// Flags + Fragment Offset
	IPPacket[7] = 0x00;

	IPPacket[8] = 0x05;		// TTL
	IPPacket[9] = protocol;		// Protocol
	IPPacket[10] = 0x00;	// Header Checksum
	IPPacket[11] = 0x00;

	IPPacket[12] = (LocalIP >> 24) & 0xFF;
	IPPacket[13] = (LocalIP >> 16) & 0xFF;
	IPPacket[14] = (LocalIP >> 8) & 0xFF;
	IPPacket[15] = (LocalIP & 0xFF);

	IPPacket[16] = (targetIP >> 24) & 0xFF;
	IPPacket[17] = (targetIP >> 16) & 0xFF;
	IPPacket[18] = (targetIP >> 8) & 0xFF;
	IPPacket[19] = (targetIP & 0xFF);

	// Options?

	// Copy the data
	int i = 0;
	int j = 0;
	for (i=20,j=0; i<IPLength; i++,j++) {
		IPPacket[i] = buffer[j];
	}

	// Calculate the checksum for just the header
	// Calculate two's complement sum of each 16bit WORD
	// (remember that network order is big-endian)
	DWORD sum = 0;
	for (i=0; i<20; i+=2) {
		sum += (IPPacket[i] << 8) + (IPPacket[i+1]);
	}
	// Calculate 16bit one's complement sum by taking the carries
	//  (topmost WORD) and adding to the result (bottommost WORD)
	WORD onesum = (WORD)(sum & 0xFFFF) + (WORD)(sum >> 16);
	// and one's complement that
	onesum = ~onesum;

	IPPacket[10] = (onesum >> 8) & 0xFF;
	IPPacket[11] = (onesum & 0xFF);

	// Give the packet to the network driver
	net_SendPacket( DestMAC, IPPacket, IPLength, FRAMETYPE_IP ); 
}
