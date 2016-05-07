/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			icmp.c
 * Description:		Internet Control Message Protocol
 * Author:			James Smith
 * Created:			02-May-2007
 * Last Modified:	04-May-2007
 *
 */
 
#include "kernel.h"
#include "ip.h"
#include "icmp.h"


WORD OnesComplementChecksum( BYTE *buffer, WORD length ) {

	int i = 0;

	// Calculate two's complement sum of each 16bit WORD
	// (remember that network order is big-endian)
	DWORD sum = 0;

	// Watch out for an odd number of bytes
	if (length & 0x01) {
		for (i=0; i<length-1; i+=2) {
			sum += (buffer[i] << 8) + (buffer[i+1]);
		}
		// Add the last one as if there is a zero on the end
		sum += buffer[i] << 8;
	} else {
		for (i=0; i<length; i+=2) {
			sum += (buffer[i] << 8) + (buffer[i+1]);
		}
	}

	// Calculate 16bit one's complement sum by taking the carries
	//  (topmost WORD) and adding to the result (bottommost WORD)
	WORD onesum = (WORD)(sum & 0xFFFF) + (WORD)(sum >> 16);
	// and one's complement that
	onesum = ~onesum;

	return onesum;
}


void icmp_SendEchoRequest( DWORD targetIP, WORD sequence ) {

	// Build an echo request
	BYTE buffer[16];

	buffer[0] = ICMP_ECHOREQUEST;   // Type 
	buffer[1] = 0x00;				// Code
	buffer[2] = 0;                  // Checksum calculated later
	buffer[3] = 0;

	buffer[4] = 0x02;					// Identifier (copied from eg)
	buffer[5] = 0x00;
	buffer[6] = (sequence >> 8) & 0xFF;  // Sequence
	buffer[7] = (sequence & 0xFF);  
	
	// Create some test data
	int i = 0;
	for (i=8; i<16; i++) {
		buffer[i] = 'a' + i;
	}

	// Calculate the checksum for the whole ICMP packet
	WORD checksum = OnesComplementChecksum( buffer, 16 );
/*
	// Calculate two's complement sum of each 16bit WORD
	// (remember that network order is big-endian)
	DWORD sum = 0;
	for (i=0; i<16; i+=2) {
		sum += (buffer[i] << 8) + (buffer[i+1]);
	}
	// Calculate 16bit one's complement sum by taking the carries
	//  (topmost WORD) and adding to the result (bottommost WORD)
	WORD onesum = (WORD)(sum & 0xFFFF) + (WORD)(sum >> 16);
	// and one's complement that
	onesum = ~onesum;
*/

	// Now store in the checksum field
	buffer[2] = (checksum >> 8) & 0xFF;
	buffer[3] = (checksum & 0xFF);

	// Give to IP layer
	ip_SendPacket( buffer, 16, targetIP, PROTOCOL_ICMP );
}


void icmp_ReceiveEchoRequest( BYTE *buffer, WORD length, DWORD sourceIP ) {
	// This a request from another host. Check it and respond.

	// Copy the request to the reply
	BYTE *reply = (BYTE*) kmalloc( length );
	memcpy( buffer, reply, length );

	// Change to a reply
	reply[0] = ICMP_ECHOREPLY;

	// Zero the checksum
	reply[2] = 0x00;
	reply[3] = 0x00;

	// Calculate a new checksum for the whole ICMP packet
	WORD checksum = OnesComplementChecksum( reply, length );

	// Now store in the checksum field
	reply[2] = (checksum >> 8) & 0xFF;
	reply[3] = (checksum & 0xFF);

	// Give to IP layer
	ip_SendPacket( reply, length, sourceIP, PROTOCOL_ICMP );

}


void icmp_PacketHandler( BYTE *buffer, WORD length, DWORD sourceIP ) {

	BYTE icmptype = buffer[0];

	// TODO: Check the checksum here.

	switch( icmptype ) {
		case ICMP_ECHOREQUEST:
			// An echo request from another host
//			con_StreamWriteString( "ECHO REQUEST\n" );
			icmp_ReceiveEchoRequest( buffer, length, sourceIP );
			break;
		case ICMP_ECHOREPLY:
			// An echo reply from a request we made
//			con_StreamWriteString( "ECHO REPLY\n" );
			break;
		case ICMP_DESTUNREACHABLE:
			break;
		default:
			// Unknown
			break;
	}
}
