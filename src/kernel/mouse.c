/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			mouse.c
 * Description:		Internal PS/2 mouse driver
 * Author:			James Smith
 * Created:			07-Sep-2006
 * Last Modified:	09-May-2007
 *
 */

#include "kernel.h"
#include "mouse.h"

#define MOUSE_SCALEX 2
#define MOUSE_SCALEY 4
#define MOUSE_SCALEZ 1
#define MOUSE_BUTTONS 8
#define MAX_BYTECOUNT 4		// Largest possible packet size

#define PORT_COMMAND 0x64	// Write
#define PORT_STATUS  0x64	// Read
#define PORT_DATA    0x60   // Read/Write

static BYTE MouseDeviceID = 0;
static int MouseDeviceByteCount = 3;

static int bytecounter = 0;
static char inputbuffer[MAX_BYTECOUNT];
static int rawxcoord = 40 * MOUSE_SCALEX;
static int rawycoord = 12 * MOUSE_SCALEY;
static int rawzcoord =  0 * MOUSE_SCALEZ;
static char rawbuttons[MOUSE_BUTTONS];
static int xcoord = 40;
static int ycoord = 12;
static int zcoord =  0;

static void KeyboardWait( void );
static void KeyboardWaitData( void );
void ENABLE_IRQ( DWORD irq );
void DISABLE_IRQ( DWORD irq );



void SendKeyboardCommandWithData( BYTE command, BYTE data ) {
	KeyboardWait();
	OUTPORT_BYTE( 0x64, command );

	KeyboardWait();
	OUTPORT_BYTE( 0x60, data );
}

BYTE SendKeyboardCommandWithResult( BYTE command ) {
	KeyboardWait();
	OUTPORT_BYTE( 0x64, command );

	KeyboardWaitData();
	return INPORT_BYTE( 0x60 );
}

void SendMouseCommand( BYTE command ) {
	BYTE result = 0x00;

	KeyboardWait();
	OUTPORT_BYTE( 0x64, 0xD4 );		// "Write Mouse Device" command

	KeyboardWait();
	OUTPORT_BYTE( 0x60, command );

	KeyboardWaitData();
	result = INPORT_BYTE( 0x60 );	// Should be 0xFA
}


void SendMouseCommandWithData( BYTE command, BYTE data ) {
	SendMouseCommand( command );
	SendMouseCommand( data );
}


BYTE SendMouseCommandWithResult( BYTE command ) {

	SendMouseCommand( command );

	KeyboardWaitData();
	return INPORT_BYTE( 0x60 );
}


int mse_InitDevice( void ) {

	int i = 0;

	// TODO: How do we tell if there is no mouse connected?

	// Enable Mouse Interrupt on IRQ12
	BYTE commandbyte = SendKeyboardCommandWithResult( 0x20 );  // "Read Command Byte"
	commandbyte |= 0x02;	        // Set bit 1    - Enable INT12
	commandbyte &= 0xDF;	        // Clear bit 5  - Enable Mouse
	SendKeyboardCommandWithData( 0x60, commandbyte );  // "Write Command Byte"

	// Turn off IRQ12 while we init
	DISABLE_IRQ( 12 );

	// Initialise the Mouse
	SendMouseCommand( 0xF4 );		// "Mouse Init" command

	// Attempt to enter "scrolling mode" for a Microsoft Intellimouse
	SendMouseCommandWithData( 0xF3, 0xC8 );		// 200
	SendMouseCommandWithData( 0xF3, 0x64 );		// 100
	SendMouseCommandWithData( 0xF3, 0x50 );		// 80

	// Attempt to enter "scrolling+buttons mode" for a Microsoft Intellimouse
	SendMouseCommandWithData( 0xF3, 0xC8 );		// 200
	SendMouseCommandWithData( 0xF3, 0xC8 );		// 200
	SendMouseCommandWithData( 0xF3, 0x50 );		// 80

	// Get the ID of the mouse
	MouseDeviceID = SendMouseCommandWithResult( 0xF2 );

	if ((MouseDeviceID == 0x03) || (MouseDeviceID == 0x04)) {
		MouseDeviceByteCount = 4;
	} else {
		MouseDeviceByteCount = 3;
	}

	// Reset some of the mouse state
	bytecounter = 0;
	for (i=0; i<MOUSE_BUTTONS; i++) {
		rawbuttons[i] = 0;
	}

	// Tell the PIC that this IRQ is ready
	ENABLE_IRQ( 12 );

	con_StreamWriteString( "[DEV] PS/2 Mouse. DeviceID=0x" );
	con_StreamWriteHexBYTE( MouseDeviceID );
	con_StreamWriteString( "\n" );

	return 0;
}


void mse_IRQHandler( void ) {

//	BYTE x = INPORT_BYTE( 0x60 );
//	con_StreamWriteString("P:");
//	con_StreamWriteHexBYTE( x );
//	inputbuffer[bytecounter++] = x;
	inputbuffer[bytecounter++] = INPORT_BYTE( 0x60 );

	if (bytecounter == MouseDeviceByteCount) {
		int oldxcoord = xcoord;
		int oldycoord = ycoord;

		// Get the values from the packet
		rawxcoord += (int)inputbuffer[1];
		rawycoord -= (int)inputbuffer[2];
		if (MouseDeviceID == 0x03) {
			rawzcoord -= (int)inputbuffer[3];
		}
		if (MouseDeviceID == 0x04) {
			rawzcoord -= (int)(inputbuffer[3] & 0x0F);
		}
		rawbuttons[0] = inputbuffer[0] & 0x01;
		rawbuttons[1] = inputbuffer[0] & 0x02;
		rawbuttons[2] = inputbuffer[0] & 0x04;
		if (MouseDeviceID == 0x04) {
			rawbuttons[3] = inputbuffer[3] & 0x10;
			rawbuttons[4] = inputbuffer[3] & 0x20;
		}

		if (rawxcoord > (79*MOUSE_SCALEX))  rawxcoord = (79*MOUSE_SCALEX);
		if (rawxcoord < 0)                  rawxcoord = 0;
		if (rawycoord > (24*MOUSE_SCALEY))  rawycoord = (24*MOUSE_SCALEY);
		if (rawycoord < 0)                  rawycoord = 0;
		xcoord = rawxcoord / MOUSE_SCALEX;
		ycoord = rawycoord / MOUSE_SCALEY;

		// Draw the mouse pointer
		if ((xcoord != oldxcoord) || (ycoord != oldycoord)) {
			BYTE *x = (BYTE*)(0xB8001 + (oldxcoord * 2) + (oldycoord * 160));
			*x = *x & 0xBF;
			BYTE *y = (BYTE*)(0xB8001 + (xcoord * 2) + (ycoord * 160));
			*y = *y | 0x40;;
		}

		// Draw the mouse wheel indicator
		if ((MouseDeviceID == 0x03) || (MouseDeviceID == 0x04)) {
			BYTE *z = (BYTE*)(0xB8030);
			*z = 0x30 + (BYTE)rawzcoord;
		}

		// Reset for the start of a new packet
		bytecounter = 0;

#if 0
		// Output the packet
		con_StreamWriteString( ">X:" );
		con_StreamWriteHexDigit( (inputbuffer[1] >> 4) & 0xF );
		con_StreamWriteHexDigit( inputbuffer[1] & 0xF );
		con_StreamWriteString( " Y:" );
		con_StreamWriteHexDigit( (inputbuffer[2] >> 4) & 0xF );
		con_StreamWriteHexDigit( inputbuffer[2] & 0xF );
		if (MouseDeviceID == 0x03) {
			con_StreamWriteString( " Z:" );
			con_StreamWriteHexDigit( (inputbuffer[3] >> 4) & 0xF );
			con_StreamWriteHexDigit( inputbuffer[3] & 0xF );
		}
		if (inputbuffer[0] & 0x01) {
			con_StreamWriteChar( 'L' );
		}
		if (inputbuffer[0] & 0x02) {
			con_StreamWriteChar( 'R' );
		}
		if (inputbuffer[0] & 0x04) {
			con_StreamWriteChar( 'M' );
		}
		if (MouseDeviceID == 0x04) {
			if (inputbuffer[3] & 0x10) {
				con_StreamWriteString( "B4" );
			}
			if (inputbuffer[3] & 0x20) {
				con_StreamWriteString( "B5" );
			}
		}
		con_StreamWriteChar( '<' );
#endif

	}
}


static void KeyboardWait( void ) {
	BYTE status = 0x02;

	// Wait for bit 2 to become 0
	while (status & 0x02) {
		status = INPORT_BYTE( 0x64 );
	}
}


static void KeyboardWaitData( void ) {
	BYTE status = 0x00;

	// Wait for bit 1 to become 1
	while (!(status & 0x01)) {
		status = INPORT_BYTE( 0x64 );
	}
}
