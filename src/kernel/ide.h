/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			ide.h
 * Description:		IDE hard disk driver (header)
 * Author:			James Smith
 * Created:			26-May-2007
 * Last Modified:	26-May-2007
 *
 */
 
#include "kernel.h"

int ide_InitDevice( void );
void ide_IRQHandler( void );
void ide_ReadSector( BYTE drive, BYTE head, WORD cylinder, BYTE sector,
					BYTE *buffer );
