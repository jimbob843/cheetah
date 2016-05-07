/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			udp.c
 * Description:		User Datagram Protocol
 * Author:			James Smith
 * Created:			10-May-2007
 * Last Modified:	10-May-2007
 *
 */
 
#include "kernel.h"
#include "udp.h"


void udp_PacketHandler( BYTE *buffer, WORD length, DWORD sourceIP ) {

	WORD sourcePort = (buffer[0] << 8) + buffer[1];
	WORD destPort = (buffer[2] << 8) + buffer[3];
	WORD messageLen = (buffer[4] << 8) + buffer[5];
	WORD messageChecksum = (buffer[6] << 8) + buffer[7];

	// Check the length and checksum (with pseudo header)

	// Find the appropriate socket

	// Add the datagram to the queue

	// Signal the waiting process
}
