/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			console.c
 * Description:		Single screen text console
 * Author:			James Smith
 * Created:			14-Aug-2006
 * Last Modified:	04-May-2007
 *
 */

#include "kernel.h"

#define COL_BLACK        0x0
#define COL_BLUE         0x1
#define COL_GREEN        0x2
#define COL_CYAN         0x3
#define COL_RED          0x4
#define COL_MAGENTA      0x5
#define COL_BROWN        0x6
#define COL_LIGHTGREY    0x7
#define COL_DARKGREY     0x8
#define COL_LIGHTBLUE    0x9
#define COL_LIGHTGREEN   0xA
#define COL_LIGHTCYAN    0xB
#define COL_LIGHTRED     0xC
#define COL_LIGHTMAGENTA 0xD
#define COL_YELLOW       0xE
#define COL_WHITE        0xF

#define TEXTVIDEO_BASEADDR (BYTE *)0xB8000
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

static BYTE colourmode = COL_WHITE | (COL_BLACK << 4);

static unsigned int cursorxposition = 0;
static unsigned int cursoryposition = 0;

void con_SetCursorPosition(unsigned int newx, unsigned int newy);
void con_ScrollUp(int numlines);
void con_IncCursorPosition(char c);
void con_WriteChar(unsigned char c);
void con_StreamWriteChar(char c);
void con_StreamWriteString(char *s);
void con_StreamWriteHexDigit(char c);
void con_StreamWriteHexDWORD(DWORD x);
void con_Reset();
void con_SetForegroundColour(BYTE c);
void con_SetBackgroundColour(BYTE c);


void con_UpdateCursor( void ) {
	// Tell the VGA adapter where to put the cursor
	DWORD cursorpos = (cursoryposition * SCREEN_WIDTH) + cursorxposition;
	OUTPORT_BYTE( 0x3D4, 0x0F );
	OUTPORT_BYTE( 0x3D5, (cursorpos & 0x00FF) );
	OUTPORT_BYTE( 0x3D4, 0x0E );
	OUTPORT_BYTE( 0x3D5, (cursorpos & 0xFF00) >> 8 );
}

void con_SetCursorPosition(unsigned int newx, unsigned int newy) {
	if ((newx >= 0) || (newx < SCREEN_WIDTH)) {
		cursorxposition = newx;
	} else {
		cursorxposition = 0;
	}
	if ((newy >= 0) || (newy < SCREEN_HEIGHT)) {
		cursoryposition = newy;
	} else {
		cursoryposition = 0;
	}

	con_UpdateCursor();
}


void con_ScrollUp(int numlines) {
	/* Scrolls the page up numlines by copying memory */

	BYTE *dest_addr = TEXTVIDEO_BASEADDR;
	BYTE *src_addr = TEXTVIDEO_BASEADDR + (numlines * SCREEN_WIDTH * 2);

	// Copy the old text up the screen by numlines
	int bytes = ((SCREEN_HEIGHT-numlines) * SCREEN_WIDTH * 2);
	memcpy( src_addr, dest_addr, bytes );
	dest_addr += bytes;

	// Clear the new lines at the bottom
	memclr( dest_addr, (numlines * SCREEN_WIDTH * 2) );

//	con_UpdateCursor();
}


void con_IncCursorPosition(char c) {
	/* Moves the cursor based on the character being printed (c) */
	if (c == 10) {	/* NL */
		if ((cursoryposition+1) == SCREEN_HEIGHT) {
			/* We're at the bottom of the screen so scroll up */
			con_ScrollUp(1);
		} else {
			cursoryposition++;
		}
		cursorxposition = 0;
		con_UpdateCursor();
		return;
	}

	if (c == 13) {	/* CR */
		return;
	}

	/* else */
	cursorxposition++;
	if (cursorxposition >= SCREEN_WIDTH) {
		/* we've hit the side, so wrap around */
		cursorxposition = 0;
		if ((cursoryposition+1) == SCREEN_HEIGHT) {
			/* We're at the bottom of the screen so scroll up */
			con_ScrollUp(1);
		} else {
			cursoryposition++;
		}
	}

	con_UpdateCursor();
}

void con_DecCursorPosition( void ) {
	cursorxposition--;
	if (cursorxposition < 0) {
		/* we've hit the side, so wrap around */
		cursorxposition = 0;
		if ((cursoryposition-1) == 0) {
			/* We're at the top of the screen so just stop */
			
		} else {
			cursoryposition;
		}
	}

	con_UpdateCursor();
}


void con_WriteChar(unsigned char c) {
	/* Writes a character to the current cursor location */

	if (c >= 0x20) {	/* Printable chars only */
		BYTE *screen_addr = TEXTVIDEO_BASEADDR;
		screen_addr += (cursorxposition * 2) + (cursoryposition * SCREEN_WIDTH * 2);
		*screen_addr++ = c;
		*screen_addr = colourmode;
	}
}



void con_StreamWriteChar(char c) {
	con_WriteChar(c);
	con_IncCursorPosition(c);
}

void con_StreamRawWriteChar(char c) {
	BYTE *screen_addr = TEXTVIDEO_BASEADDR;
	screen_addr += (cursorxposition * 2) + (cursoryposition * SCREEN_WIDTH * 2);
	*screen_addr++ = c;
	*screen_addr = colourmode;
	con_IncCursorPosition(0x00);
}

void con_StreamBackspaceChar( void ) {
	con_DecCursorPosition();
	con_WriteChar(' ');
}


void con_StreamWriteString(char *s) {
	char c = *s;
	while (c != 0) {
		con_StreamWriteChar(c);
		s++;
		c = *s;
	}
}

void con_StreamWriteHexDigit(char c) {
	/* Input: 0-F, prints char */
	char x = '0';

	if ((c < 0) || (c > 0xF)) {
		/* Bad char */
		x = '0';
	} else {
		if ((c >= 0) && (c <= 9)) {
			x = '0' + c;
		} else {
			x = '0' + c + 7;
		}
	}

	con_StreamWriteChar( x );
}

void con_StreamWriteHexDWORD(DWORD x) {
	int i = 0;
	int shift = 32;		// 32 bits in total

	// Print each nibble of the DWORD, starting at the top
	for (i=0; i<8; i++) {
		shift -= 4;		// Move to next nibble
		con_StreamWriteHexDigit( (0xF & (x >> shift)) );
	}
}

void con_StreamWriteDecDWORD(DWORD x) {
	BYTE revdigits[12];
	DWORD current = x;
	int i = 0;
	int j = 0;

	while (current>0) {
		revdigits[i] = current % 10;
		current /= 10;
		i++;
	}

	for (j=i; j>0; j--) {
		con_StreamWriteHexDigit( revdigits[j-1] );
	}

}

void con_StreamWriteHexBYTE(BYTE x) {
	con_StreamWriteHexDigit( (x & 0xF0) >> 4 );
	con_StreamWriteHexDigit( (x & 0x0F) );
}
		

void con_Reset() {
	/* Clears the screen, and sets the cursor to top-left (0,0) */

	BYTE *screen_addr = TEXTVIDEO_BASEADDR;
	int i = 0;

	for (i=0; i<(SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
		*screen_addr++ = ' ';
		*screen_addr++ = colourmode;
	}

	con_SetCursorPosition(0,0);
}

void con_SetForegroundColour(BYTE c) {
	colourmode = (colourmode & 0xF0) + (c & 0x0F);
}

void con_SetBackgroundColour(BYTE c) {
	colourmode = ((c<<4) & 0xF0) + (colourmode & 0x0F);
}


void con_DumpMemoryBlock( BYTE *addr, BYTE maxlines ) {
	// Prints a 512 byte block to the screen
	int i = 0, j = 0;
	for (j=0; j<maxlines; j++) {
		con_StreamWriteHexBYTE( j );
		con_StreamWriteString( ": " );
		for (i=0; i<32; i++) {
			con_StreamWriteHexBYTE( addr[i + (j*32)] );
			if (((i+1) % 8) == 0) {
				con_StreamWriteChar( ' ' );
			}
		}
		con_StreamWriteChar( '\n' );
	}
}
