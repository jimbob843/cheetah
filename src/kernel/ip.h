/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			ip.h
 * Description:		Internet Protocol (header)
 * Author:			James Smith
 * Created:			29-Apr-2007
 * Last Modified:	04-May-2007
 *
 */
 
#include "kernel.h"

#define PROTOCOL_ICMP	0x01
#define PROTOCOL_UDP	0x11
#define PROTOCOL_TCP	0x06

void ip_PacketHandler( BYTE *buffer, WORD length );
void ip_ProcessPacket( BYTE *buffer, WORD length );
void ip_SendPacket( BYTE *buffer, WORD length, DWORD targetIP,
				   BYTE protocol );
