/*
 * Oyombo v1.0   Copyright Marlet Limited 2006
 *
 * File:			arp.h
 * Description:		Address Resolution Protocol (header)
 * Author:			James Smith
 * Created:			28-Dec-2007
 * Last Modified:	28-Dec-2007
 *
 */
 
#include "kernel.h"

struct _ArpEntry {
	DWORD IPAddress;
	BYTE MACAddress[6];
	struct _ArpEntry *NextEntry;
};
typedef struct _ArpEntry ArpEntry;


void arp_GenerateRequest( DWORD targetIP );
void arp_GenerateReply( BYTE *request, WORD length );
void arp_HandleReply( BYTE *buffer, WORD length );
void arp_PacketHandler( BYTE *buffer, WORD length );
void arp_AddEntry( DWORD ip, BYTE *mac );
BYTE *arp_LookupMAC( DWORD ip );
void arp_DumpARPTable( void );
