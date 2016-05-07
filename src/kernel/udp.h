/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			udp.h
 * Description:		User Datagram Protocol (Header)
 * Author:			James Smith
 * Created:			10-May-2007
 * Last Modified:	10-May-2007
 *
 */
 
#include "kernel.h"

void udp_PacketHandler( BYTE *buffer, WORD length, DWORD sourceIP );
