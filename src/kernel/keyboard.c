/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			keyboard.c
 * Description:		Internal PS/2 keyboard driver
 * Author:			James Smith
 * Created:			15-Aug-2006
 * Last Modified:	01-May-2007
 *
 */

#include "kernel.h"

// NB: No support yet for extended keycodes

void sch_DumpProcessList( void );
void sch_ProcessNotify( Event *w );


static Device keyboard_device;
static unsigned char RawKeyboardBuffer[10];		// Storage for partial codes
static unsigned char ASCIIKeyboardBuffer[256];	// Storage for translated characters
static int ASCIIBufferStart = 0;
static int ASCIIBufferEnd = 0;
static int Shifted = 0;

void key_PutCharInBuffer( unsigned char c );


// Keymap translates from keycodes to ASCII
static unsigned char KeyboardMapUnshifted[] = {
	0,		// 00 Error?
	0,		// 01 ESC
	'1',	// 02
	'2',	// 03
	'3',	// 04
	'4',	// 05
	'5',	// 06
	'6',	// 07
	'7',	// 08
	'8',	// 09
	'9',	// 0A
	'0',	// 0B
	'-',	// 0C
	'=',	// 0D
	8,		// 0E BACKSPACE
	9,		// 0F TAB
	'q',	// 10
	'w',	// 11
	'e',	// 12
	'r',	// 13
	't',	// 14
	'y',	// 15
	'u',	// 16
	'i',	// 17
	'o',	// 18
	'p',	// 19
	'[',	// 1A
	']',	// 1B
	'\n',	// 1C RETURN
	0,		// 1D LCTRL
	'a',	// 1E
	's',	// 1F
	'd',	// 20
	'f',	// 21
	'g',	// 22
	'h',	// 23
	'j',	// 24
	'k',	// 25
	'l',	// 26
	';',	// 27
	39,		// 28 '
	96,		// 29 `
	0,		// 2A LSHIFT
	'#',	// 2B
	'z',	// 2C
	'x',	// 2D
	'c',	// 2E
	'v',	// 2F
	'b',	// 30
	'n',	// 31
	'm',	// 32
	',',	// 33
	'.',	// 34
	'/',	// 35
	0,		// 36 RSHIFT
	'*',	// 37 KEYPAD *
	0,		// 38 LALT
	' ',	// 39
	0,		// 3A CAPSLOCK
	128,		// 3B F1
	129,		// 3C F2
	130,		// 3D F3
	131,		// 3E F4
	132,		// 3F F5
	133,		// 40 F6
	134,		// 41 F7
	135,		// 42 F8
	136,		// 43 F9
	137,		// 44 F10
	0,		// 45 NUMLOCK
	0,		// 46 SCROLLLOCK
	'7',	// 47
	'8',	// 48
	'9',	// 49
	'-',	// 4A
	'4',	// 4B
	'5',	// 4C
	'6',	// 4D
	'+',	// 4E
	'1',	// 4F
	'2',	// 50
	'3',	// 51
	'0',	// 52
	'.',	// 53
	0,		// 54 Not used
	0,		// 55 Not used
	'\\'	// 56
};

static unsigned char KeyboardMapShifted[] = {
	0,		// 00 Error?
	0,		// 01 ESC
	'!',	// 02
	'\"',	// 03
	156,	// 04
	'$',	// 05
	'%',	// 06
	'^',	// 07
	'&',	// 08
	'*',	// 09
	'(',	// 0A
	')',	// 0B
	'_',	// 0C
	'+',	// 0D
	8,		// 0E BACKSPACE
	9,		// 0F TAB
	'Q',	// 10
	'W',	// 11
	'E',	// 12
	'R',	// 13
	'T',	// 14
	'Y',	// 15
	'U',	// 16
	'I',	// 17
	'O',	// 18
	'P',	// 19
	'{',	// 1A
	'}',	// 1B
	'\n',	// 1C RETURN
	0,		// 1D LCTRL
	'A',	// 1E
	'S',	// 1F
	'D',	// 20
	'F',	// 21
	'G',	// 22
	'H',	// 23
	'J',	// 24
	'K',	// 25
	'L',	// 26
	':',	// 27
	'@',	// 28
	170,	// 29
	0,		// 2A LSHIFT
	'~',	// 2B
	'Z',	// 2C
	'X',	// 2D
	'C',	// 2E
	'V',	// 2F
	'B',	// 30
	'N',	// 31
	'M',	// 32
	'<',	// 33
	'>',	// 34
	'?',	// 35
	0,		// 36 RSHIFT
	'*',	// 37 KEYPAD *
	0,		// 38 LALT
	' ',	// 39
	0,		// 3A CAPSLOCK
	138,		// 3B F1
	139,		// 3C F2
	140,		// 3D F3
	141,		// 3E F4
	142,		// 3F F5
	143,		// 40 F6
	144,		// 41 F7
	145,		// 42 F8
	146,		// 43 F9
	147,		// 44 F10
	0,		// 45 NUMLOCK
	0,		// 46 SCROLLLOCK
	'7',	// 47
	'8',	// 48
	'9',	// 49
	'-',	// 4A
	'4',	// 4B
	'5',	// 4C
	'6',	// 4D
	'+',	// 4E
	'1',	// 4F
	'2',	// 50
	'3',	// 51
	'0',	// 52
	'.',	// 53
	0,		// 54 Not used
	0,		// 55 Not used
	'|'	// 56
};


int key_InitDevice( void ) {
	keyboard_device.DataReady.EventStatus = FALSE;

	con_StreamWriteString( "[DEV] PS/2 Keyboard\n" );
	return 0;
}

void key_IRQHandler( void )
{
	// Keyboard interrupt handler
	//  Accept each byte from the keyboard and convert it
	//  to a ASCII character.

	unsigned char x = INPORT_BYTE( 0x60 );
	unsigned char ascii = 0;

	if (x == 0x3B) {
		// F1
		sch_DumpProcessList();
		return;
	}

	// Translate the keyboard code
	if (x <= 0x56) {
		// Look it up in the table
		if (Shifted == 0) {
			ascii = KeyboardMapUnshifted[x];
		} else {
			ascii = KeyboardMapShifted[x];
		}
		if (ascii != 0) {
			key_PutCharInBuffer( ascii );
			sch_ProcessNotify( (Event *) 0x00000001 );
		}
	}
	if ((x == 0x2A) || (x == 0x36)) {
		// SHIFT DOWN
		Shifted = 1;
	}
	if ((x == 0xAA) || (x == 0xB6)) {
		// SHIFT UP
		Shifted = 0;
	}
}


int key_GetNumCharsInBuffer( void ) {

	if (ASCIIBufferStart == ASCIIBufferEnd) {
		return 0;
	}

	if (ASCIIBufferStart < ASCIIBufferEnd) {
		return ASCIIBufferEnd - ASCIIBufferStart;
	}

	if (ASCIIBufferStart > ASCIIBufferEnd) {
		return 256 + ASCIIBufferEnd - ASCIIBufferStart;
	}

	return 0;
}


unsigned char key_GetCharFromBuffer( void ) {
	unsigned char c = 0;
	if (ASCIIBufferStart == ASCIIBufferEnd) {
		// Nothing in the buffer
		c = 0;
	} else {
		c = ASCIIKeyboardBuffer[ASCIIBufferStart++];
		if (ASCIIBufferStart >= 256) {
			ASCIIBufferStart = 0;
		}
	}
	return c;
}

void key_PutCharInBuffer( unsigned char c ) {
	ASCIIKeyboardBuffer[ASCIIBufferEnd++] = c;
	if (ASCIIBufferEnd >= 256) {
		// Wrap around to the start
		ASCIIBufferEnd = 0;
	}
	// Inform any waiting processes that there are new characters
	//  in the buffer.
	keyboard_device.DataReady.EventStatus = TRUE;
}

unsigned char getc( void ) {
	// Gets an ascii character from the keyboard buffer, wait otherwise

	unsigned char c = 0;
	while (key_GetNumCharsInBuffer() == 0) {
		sch_ProcessWait((DWORD)0x00000001);
	}

	return key_GetCharFromBuffer();
}
