/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			icmp.h
 * Description:		Internet Control Message Protocol (header)
 * Author:			James Smith
 * Created:			02-May-2007
 * Last Modified:	04-May-2007
 *
 */
 
#include "kernel.h"

#define ICMP_ECHOREPLY       0x00
#define ICMP_DESTUNREACHABLE 0x01
#define ICMP_ECHOREQUEST     0x08

void icmp_SendEchoRequest( DWORD targetIP, WORD sequence );
void icmp_PacketHandler( BYTE *buffer, WORD length, DWORD sourceIP );
