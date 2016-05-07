/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			vga.h
 * Description:		Video Graphics Adapter (Header)
 * Author:			James Smith
 * Created:			18-May-2007
 * Last Modified:	18-May-2007
 *
 */
 
#include "kernel.h"

void vga_SetMode( DWORD modeIndex );
void vga_DrawPixel( WORD x, WORD y, BYTE colour );
void vga_DrawLine( WORD x1, WORD y1, WORD x2, WORD y2, BYTE colour );
void vga_DrawRectangle( WORD x1, WORD y1, WORD x2, WORD y2, BYTE colour );
