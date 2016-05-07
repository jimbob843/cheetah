/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			ne2000.h
 * Description:		NE2000 network driver (header)
 * Author:			James Smith
 * Created:			20-Apr-2007
 * Last Modified:	02-May-2007
 *
 */
 
#include "kernel.h"

#define FRAMETYPE_ARP 0x0806
#define FRAMETYPE_IP  0x0800

int net_InitDevice( void );
void net_IRQHandler( void );
void net_ReadData( BYTE *data, WORD *length );
void net_ReadPacket( BYTE *data, WORD *length );
void net_SendPacket( BYTE *destMAC, BYTE *data, WORD length, WORD frameType );
void net_GetLocalMAC( BYTE *localMAC );



