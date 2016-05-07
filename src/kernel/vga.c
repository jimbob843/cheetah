/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			vga.c
 * Description:		Video Graphics Adapter
 * Author:			James Smith
 * Created:			18-May-2007
 * Last Modified:	18-May-2007
 *
 */
 
#include "kernel.h"
#include "vga.h"

static BYTE CurrentColour = 0x00;

// Attribute Controller
#define VGA_AC_ADDRESS      0x3C0
#define VGA_AC_DATA_WRITE   0x3C0
#define VGA_AC_DATA_READ    0x3C1

// Miscellaneous Output
#define VGA_MISC_WRITE      0x3C2
#define VGA_MISC_READ       0x3CC

// Sequencer
#define VGA_SEQ_ADDRESS     0x3C4
#define VGA_SEQ_DATA        0x3C5

// DAC
#define VGA_DAC_STATE       0x3C7
#define VGA_DAC_READ_MODE   0x3C7
#define VGA_DAC_WRITE_MODE  0x3C8
#define VGA_DAC_DATA        0x3C9

// Graphics
#define VGA_GFX_ADDRESS     0x3CE
#define VGA_GFX_DATA        0x3CF

// CRT Controller  (Colour)
#define VGA_CRT_ADDRESS     0x3D4
#define VGA_CRT_DATA        0x3D5

#define VGA_INSTAT_READ     0x3DA


#define GFX_OPERATION_NONE  0x00
#define GFX_OPERATION_AND   0x08
#define GFX_OPERATION_OR    0x10
#define GFX_OPERATION_XOR   0x18
#define GFX_WRITE_MODE0     0x00
#define GFX_WRITE_MODE1     0x01
#define GFX_WRITE_MODE2     0x02
#define GFX_WRITE_MODE3     0x03


// 80 bytes per line, 480 lines, 4 planes

unsigned char g_640x480x16[] =
{
/* MISC */
	0xE3,  // 7:vsync+, 6:hsync+, 5:highpage, 3-2:25MHz clock, 1:RAM Enable, 0:CRTC@3Dx 
/* SEQ */
	0x03,  // 1:syncreset, 0:asyncreset => SEQ on.
	0x01,  // 0:8dots/char 3:dotclocknormal 5:screendisable=off
	0x08,  // map mask, 4 planes on.
	0x00,  // Charselect. Both A&B @ 0x1000
	0x06,  // 1:extendedmem, 2:O/E disable, 3:Chain 4=off
/* CRTC */
	// All horizontal in 8 dot chars
	0x5F,  // Horizontal Total = (0x5F + 5)*8 = 800
	0x4F,  // End Horizontal   = (0x4F + 1)*8 = 640
	0x50,  // Start Horiz Blank = (0x50)*8 = 640  (no +1?)
	0x82,  // End Horiz Blank = (2+32) = 34, 7:lightpen=1 (always 1)
	0x54,  // Start Horiz Retrace = (0x54)*8 = 672
	0x80,  // End Horiz Retrace = 0, 7:End Blank bit5 => +32
	0x0B,  // Vertical Total   = (0x0B) = 11 + 512 = 523
	0x3E,  // Overflow
		   // 7: Start Vert Retrace bit9  = 0
		   // 6: End Vertical bit9        = 0
 		   // 5: Vertical Total bit9      = 1 +512
		   // 4: Line Compate bit8        = 1 +256
		   // 3: Start Vert Blank bit8    = 1 +256
		   // 2: Start Vert Retrace bit8  = 1 +256
		   // 1: End Vertical bit8        = 1 +256
		   // 0: Vertical Total bit8      = 0      

	0x00,  // Preset Row Scan
	0x40,  // Maximum Scanline 
	       // 7: Scan Doubling = 0
		   // 6: Line Compare bit9 = 1 + 512
		   // 5: Start Vert Blank bit9 = 0
		   // 4-0: Max Scanline = 0
	0x00,  // Cursor Start, bit5=enabled, bit4-0=ScanLineStart
	0x00,  // Cursor End
	0x00,  // Start Address High bit8-15
	0x00,  // Start Address Low bit0-7
	0x00,  // Cursor Location High
	0x00,  // Cursor Location Low

	0xEA,  // Start Vert Retrace = (0xEA) = 234 + 256 = 490
	0x0C,  // End Vert Retrace = 12, 7:VGAProtect=off, 6:Bandwidth?
	0xDF,  // End Vertical = 223 + 256 = 479
	0x28,  // Offset = 40 bytes per scanline
	0x00,  // Underline Location 
	0xE7,  // Start Vert Blank = (0xE7) = 231 + 256 = 487
	0x04,  // End Vert Blank = 4 ???
	0xE3,  // Mode Control

	0xFF,  // Line Compare (off)
/* GC */
	0x00,  // Set/Reset
	0x00,  // Enable Set/Reset
	0x00,  // Colour Compare
	0x00,  // Data Rotate
	0x03,  // Read Map Select (plane 3?)
	0x00,  // Graphics Mode
	0x05,  // Misc, 0:Alpha, 1:Chain4=off, 2-3:B0000-B7FFF
	0x0F,  // Colour Don't Care
	0xFF,  // Bit Mask
/* AC */
	0x00,  // Internal Palette Index 0
	0x01,  //    "        "      "   1
	0x02,  //    "        "      "   2
	0x03,  //    "        "      "   3
	0x04,  //    "        "      "   4
	0x05,  //    "        "      "   5
	0x14,  //    "        "      "   6
	0x07,  //    "        "      "   7

	0x38,  //    "        "      "   8
	0x39,  //    "        "      "   9
	0x3A,  //    "        "      "   10
	0x3B,  //    "        "      "   11
	0x3C,  //    "        "      "   12
	0x3D,  //    "        "      "   13
	0x3E,  //    "        "      "   14
	0x3F,  //    "        "      "   15

	0x01,  // Attribute Mode Control, bit0:graphics
	0x00,  // Overscan Colour
	0x0F,  // Colour Plane Enable
	0x00,  // Horizontal Pixel Panning
	0x00   // Colour Select
};

// ModeLine: 640 672 x 800  479 490 x 523  +vsync +hsync
// Where is the pixel clock set?
//
// HDisplay    = 640   (End Horizontal)
// HStart      = 672   (Start Horiz Retrace)
// HEnd        = 0     (End Horiz Retrace)
// HTotal      = 800   (Horizontal Total)
// HBlankStart = 640   (Start Horiz Blank)  = HDisplay?
// HBlankEnd   = 34    (End Horiz Blank)    = HTotal?
//
// VDisplay    = 479   (End Vertical)
// VStart      = 490   (Start Vert Retrace)
// VEnd        = 12    (End Vert Retrace)
// VTotal      = 523   (Vertical Total)
// VBlankStart = 487   (Start Vert Blank)   = VDisplay?
// VBlankEnd   = 4     (End Vert Blank)     = VTotal?
//
//   0 Zero
//   0 Display Enable Skew
// 640 End Display
// 640 Start Blanking
// 672 Start Retrace
// 672 Start Retrace + End Retrace
// 674 Start Blanking + End Blanking
// 800 Total
//


void vga_SetMode( DWORD modeIndex ) {

	OUTPORT_BYTE( VGA_MISC_WRITE, 0xE3 );

	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x00 );
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x03 );
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x01 );
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x01 );
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x08 );
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x03 );
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x00 );
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x04 );
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x06 );

	// Unlock CRTC regsiters
/* unlock CRTC registers */
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x03);
	OUTPORT_BYTE( VGA_CRT_DATA, INPORT_BYTE( VGA_CRT_DATA ) | 0x80);
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x11);
	OUTPORT_BYTE( VGA_CRT_DATA, INPORT_BYTE( VGA_CRT_DATA ) & ~0x80);

	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x00 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x5F );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x01 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x4F );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x02 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x50 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x03 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x82 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x04 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x54 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x05 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x80 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x06 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x0B );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x07 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x3E );

	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x08 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x09 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x40 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x0A );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x0B );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x0C );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x0D );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x0E );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x0F );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );

	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x10 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0xEA );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x11 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x0C );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x12 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0xDF );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x13 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x28 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x14 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x00 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x15 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0xE7 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x16 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0x04 );
	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x17 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0xE3 );

	OUTPORT_BYTE( VGA_CRT_ADDRESS, 0x18 );
	OUTPORT_BYTE( VGA_CRT_DATA, 0xFF );

	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x00 );
	OUTPORT_BYTE( VGA_GFX_DATA, 0x00 );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x01 );
	OUTPORT_BYTE( VGA_GFX_DATA, 0x00 );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x02 );
	OUTPORT_BYTE( VGA_GFX_DATA, 0x00 );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x03 );
	OUTPORT_BYTE( VGA_GFX_DATA, GFX_OPERATION_NONE );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x04 );
	OUTPORT_BYTE( VGA_GFX_DATA, 0x03 );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x05 );
	OUTPORT_BYTE( VGA_GFX_DATA, GFX_WRITE_MODE3 );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x06 );
	OUTPORT_BYTE( VGA_GFX_DATA, 0x05 );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x07 );
	OUTPORT_BYTE( VGA_GFX_DATA, 0x0F );

	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x08 );
	OUTPORT_BYTE( VGA_GFX_DATA, 0xFF );

	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x00 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x00 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x01 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x01 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x02 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x02 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x03 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x03 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x04 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x04 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x05 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x05 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x06 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x14 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x07 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x07 );

	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x08 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x38 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x09 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x39 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x0A );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x3A );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x0B );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x3B );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x0C );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x3C );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x0D );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x3D );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x0E );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x3E );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x0F );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x3F );

	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x10 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x01 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x11 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x00 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x12 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x0F );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x13 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x00 );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x14 );
	OUTPORT_BYTE( VGA_AC_DATA_WRITE, 0x00 );

	BYTE *video = (BYTE *)0xA0000;
	int i = 0;

#if 0
	video = (BYTE*)0xA0000;
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x01 );
	for (i=0; i<80*480*1; i++) {
		*video++ = 0x00;
	}

	video = (BYTE*)0xA0000;
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x02 );
	for (i=0; i<80*480*1; i++) {
		*video++ = 0x00;
	}

	video = (BYTE*)0xA0000;
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x04 );
	for (i=0; i<80*480*1; i++) {
		*video++ = 0x00;
	}
#endif

	video = (BYTE*)0xA0000;
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x0F );
	for (i=0; i<80*480*1; i++) {
		*video++ = 0x00;
	}

	INPORT_BYTE( VGA_INSTAT_READ );
	OUTPORT_BYTE( VGA_AC_ADDRESS, 0x20 );


}

// Plane     Bits
//        0 1 2 3 4 5 6 7    New
//     0  X X X X X   X X    -
//     1    X     X          X
//     2  X X         X      X
//     3    X X   X   X X    X
//
// New    - - - - X - X -   Colour    
//        0 0 0 0 1 0 1 0
//
// Use OR to set a bit in a plane, and AND with NOTted data to clear


void vga_DrawPixel( WORD x, WORD y, BYTE colour ) {

	if (x >= 640)  return;
	if (y >= 480)  return;

	if (colour != CurrentColour) {
		// Need to switch colour
		// Only 16 colours supported
		OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x00 );  // Set/Reset Value
		OUTPORT_BYTE( VGA_GFX_DATA, colour & 0x0F );
		OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x01 );  // Set/Reset Enable
		OUTPORT_BYTE( VGA_GFX_DATA, 0x0F );
		OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
		OUTPORT_BYTE( VGA_SEQ_DATA, 0x0F );

		CurrentColour = colour;
	}

	// Calculate the pixel position
	// 640x480 = 80 bytes per line
	DWORD offset = (y * 80) + (x >> 3);
	BYTE value = 0x80 >> (x & 0x07);

	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x08 );  // Bit mask
	OUTPORT_BYTE( VGA_GFX_DATA, value );

	BYTE *video = (BYTE*)0xA0000 + offset;
	*video |= value;

	// TODO: This shouldn't be a OR should it?!?
}


static int abs( int x ) {
	if (x<0) {
		return -x;
	} else {
		return x;
	}
}

static int sgn( int x ) {
	if (x<0) {
		return -1;
	} else {
		return 1;
	}
}

void vga_DrawLine( WORD x1, WORD y1, WORD x2, WORD y2, BYTE colour ) {

	// Draw a line using Bresenham's algorithm

	int CurrentX = x1;
	int CurrentY = y1;
	int DeltaX = abs( x2 - x1 );
	int DeltaY = abs( y2 - y1 );

	int DeltaXSgn = sgn( x2 - x1 );
	int DeltaYSgn = sgn( y2 - y1 );
	int x = DeltaX >> 1;
	int y = DeltaY >> 1;
	int i = 0;

	vga_DrawPixel( CurrentX, CurrentY, colour );

	if (DeltaX >= DeltaY) {
		// The line is more horizontal than vertical
		for(i=0; i<DeltaX; i++) {
			y += DeltaY;
			if (y >= DeltaX) {
				y -= DeltaX;
				CurrentY += DeltaYSgn;
			}
			CurrentX += DeltaXSgn;
			vga_DrawPixel( CurrentX, CurrentY, colour );
		}

	} else {
		// The line is more vertical than horizontal
		for (i=0; i<DeltaY; i++) {
			x += DeltaX;
			if (x >= DeltaY)
			{
				x -= DeltaY;
				CurrentX += DeltaXSgn;
			}
			CurrentY += DeltaYSgn;
			vga_DrawPixel( CurrentX, CurrentY, colour );
		}
	}


#if OLD
	int dx = x2 - x1;
	int dy = y2 - y1;
	int dxabs = abs(dx);
	int dyabs = abs(dy);
	int sdx = sgn(dx);
	int sdy = sgn(dy);
	int x = dyabs >> 1;
	int y = dxabs >> 1;
	int px = x1;
	int py = y1;
	int i = 0;

	vga_DrawPixel( px, py, colour );

	if (dxabs >= dyabs) {
		// The line is more horizontal than vertical
		for(i=0; i<dxabs; i++) {
			y += dyabs;
			if (y >= dxabs) {
				y -= dxabs;
				py += sdy;
			}
			px += sdx;
			vga_DrawPixel( px, py, colour );
		}

	} else {
		// The line is more vertical than horizontal
		for (i=0; i<dyabs; i++) {
			x += dxabs;
			if (x >= dyabs)
			{
				x -= dyabs;
				px += sdx;
			}
			py += sdy;
			vga_DrawPixel( px, py, colour );
		}
	}
#endif
}


void vga_DrawRectangle( WORD x1, WORD y1, WORD x2, WORD y2, BYTE colour ) {

	vga_DrawLine( x1, y1, x2, y1, colour );
	vga_DrawLine( x2, y1, x2, y2, colour );
	vga_DrawLine( x2, y2, x1, y2, colour );
	vga_DrawLine( x1, y2, x1, y1, colour );
}


void vga_DrawFilledRectangle( WORD x1, WORD y1, WORD x2, WORD y2, BYTE colour ) {

	WORD y = 0;
	WORD x = 0;
	DWORD offset = 0;
	BYTE value = 0;
	BYTE *video = 0;

	if (colour != CurrentColour) {
		// Need to switch colour
		// Only 16 colours supported
		OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x00 );  // Set/Reset Value
		OUTPORT_BYTE( VGA_GFX_DATA, colour & 0x0F );
		OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x01 );  // Set/Reset Enable
		OUTPORT_BYTE( VGA_GFX_DATA, 0x0F );
		OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
		OUTPORT_BYTE( VGA_SEQ_DATA, 0x0F );

		CurrentColour = colour;
	}

	// Draw left side
	offset = (y1 * 80);
	value = 0xFF >> (x1 & 0x07);
	video = (BYTE*)0xA0000 + offset + (x1 >> 3);

	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x08 );  // Bit mask
	OUTPORT_BYTE( VGA_GFX_DATA, value );

	for (y=y1; y<=y2; y++) {
		*video |= value;
		video += 80;
	}

	// Draw centre
	offset = (y1 * 80);

	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x08 );  // Bit mask
	OUTPORT_BYTE( VGA_GFX_DATA, 0xFF );

	for (x=x1+8; x<x2; x+=8) {
		video = (BYTE*)0xA0000 + offset + (x >> 3);
		for (y=y1; y<=y2; y++) {
			*video |= 0xFF;
			video += 80;
		}
	}

	// Draw right side

}


void vga_FillScreen( BYTE colour ) {

	// TODO: Use GR00 to set/reset planes instead.

	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x00 );  // Set/Reset Value
	OUTPORT_BYTE( VGA_GFX_DATA, colour & 0x0F );
	OUTPORT_BYTE( VGA_GFX_ADDRESS, 0x01 );  // Set/Reset Enable
	OUTPORT_BYTE( VGA_GFX_DATA, 0x0F );
	OUTPORT_BYTE( VGA_SEQ_ADDRESS, 0x02 );  // Map Mask
	OUTPORT_BYTE( VGA_SEQ_DATA, 0x0F );

	int i = 0;
	DWORD *video = (DWORD*)0xA0000;
	for (i=0; i<80*480*1/4; i++) {
		*video++ = 0xFFFFFFFF;
	}
}


void vga_SetPalette( BYTE colour, BYTE red, BYTE green, BYTE blue ) {

	// TODO: This doesn't work!
	OUTPORT_BYTE( VGA_DAC_WRITE_MODE, colour );
	OUTPORT_BYTE( VGA_DAC_DATA, red );
	OUTPORT_BYTE( VGA_DAC_DATA, green );
	OUTPORT_BYTE( VGA_DAC_DATA, blue );
}


