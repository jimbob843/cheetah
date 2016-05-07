/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			pci.c
 * Description:		PCI Interface
 * Author:			James Smith
 * Created:			10-Sep-2006
 * Last Modified:	01-May-2007
 *
 */
 
#include "kernel.h"

extern DWORD global_NE2000Addr;

#define PORT_PCI_CONFIG_ADDR 0xCF8  // DWORD port (0xCF8-0xCFB)
#define PORT_PCI_CONFIG_DATA 0xCFC  // DWORD port (0xCFC-0xCFF)

#define NUM_PCI_BUS       8  // ????
#define NUM_PCI_DEVICE   32  // TODO: Double check. Was 64!
#define NUM_PCI_FUNCTION  8


static DWORD GetConfigDWORD( BYTE bus, BYTE device, BYTE function, BYTE addr ) {
	// bus (0-255), device (0-31), function (0-7), addr (0-63)

	// Create the configuration address
	DWORD configaddr = 0x80000000;	// bit 31 must be 1 for config. (0 means PCI IO transaction)

	// Put the values into the config addr
	configaddr |= (bus      & 0xFF) << 16;
	configaddr |= (device   & 0x1F) << 11;
	configaddr |= (function & 0x07) <<  8;

	// addr must be DWORD aligned, so bottom two bits are zeros.
	configaddr |= (addr     & 0xFC);

	// Get the current config addr (Do we need this?)
	DWORD origaddr = INPORT_DWORD( PORT_PCI_CONFIG_ADDR );

	// Perform out config transaction
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, configaddr );		// Send address
	DWORD value = INPORT_DWORD( PORT_PCI_CONFIG_DATA );		// Read value

	// Put back the original config addr
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, origaddr );

	return value;
}



void pci_DeviceScan( void ) {

	// Interrogate the PCI configuration space to determine what
	//  buses/devices are available. The config space is a special
	//  memory area that is accessed through the ports.
	
	BYTE bus = 0;
	BYTE device = 0;
	BYTE function = 0;

	for (bus=0; bus<NUM_PCI_BUS; bus++) {
		for (device=0; device<NUM_PCI_DEVICE; device++) {

			DWORD x = GetConfigDWORD( bus, device, 0, 0x00 );
			DWORD vendorid = x & 0xFFFF;

			if ((vendorid == 0x0000) || (vendorid == 0xFFFF)) {
				// Not a device
			} else {
				BYTE functioncount = 1;		// Default to single function device
				DWORD y = GetConfigDWORD( bus, device, 0, 0x0C );
				BYTE headertype = (y & 0x00FF0000) >> 16;
				if (headertype & 0x80) {
					// Is multi-function device
					functioncount = NUM_PCI_FUNCTION;
				}

				for (function=0; function<functioncount; function++) {
					DWORD z = GetConfigDWORD( bus, device, function, 0x00 );
					DWORD vendorid = z & 0xFFFF;
					if ((vendorid == 0x0000) || (vendorid == 0xFFFF)) {
						// No device
					} else {
						DWORD tmpclass = 0;
						// Get some basic PCI info
						con_StreamWriteString( "[PCI] B=" );
						con_StreamWriteHexBYTE( bus );
						con_StreamWriteString( " D=" );
						con_StreamWriteHexBYTE( device );
						con_StreamWriteString( " F=" );
						con_StreamWriteHexBYTE( function );
						con_StreamWriteString( " Vendor=" );
						con_StreamWriteHexDWORD( 
							GetConfigDWORD( bus, device, function, 0x00 ) );
						con_StreamWriteString( " Class=" );
						tmpclass = GetConfigDWORD( bus, device, function, 0x08 );
						con_StreamWriteHexDWORD( tmpclass );
						con_StreamWriteString( " Int=" );
						con_StreamWriteHexDWORD( 
							GetConfigDWORD( bus, device, function, 0x3C ) );
						switch ((tmpclass & 0xFF000000) >> 24) {
							case 0x00:
								con_StreamWriteString( " Pre PCI rev 2.0" );
								break;
							case 0x01:
								con_StreamWriteString( " Mass Storage" );
								break;
							case 0x02:
								con_StreamWriteString( " Network" );
								break;
							case 0x03:
								con_StreamWriteString( " Video" );
								break;
							case 0x04:
								con_StreamWriteString( " Multimedia" );
								break;
							case 0x06:
								con_StreamWriteString( " Bridge" );
								break;
							case 0x07:
								con_StreamWriteString( " Simple Comms" );
								break;
							case 0x08:
								con_StreamWriteString( " Base System" );
								break;
							case 0x09:
								con_StreamWriteString( " Input" );
								break;
							case 0x0A:
								con_StreamWriteString( " Docking Station" );
								break;
							case 0x0B:
								con_StreamWriteString( " Processor" );
								break;
							case 0x0C:
								con_StreamWriteString( " Serial Bus" );
								break;
							case 0x0D:
								con_StreamWriteString( " Wireless" );
								break;
							case 0x0E:
								con_StreamWriteString( " Intelligent IO" );
								break;
							case 0x0F:
								con_StreamWriteString( " Satellite Comms" );
								break;
							default:
								con_StreamWriteString( " Unknown" );
						}
						con_StreamWriteChar( '\n' );

						if (vendorid == 0x10EC) {
							// Realtek, dump address info
							con_StreamWriteString( "[PCI]   ADDR:" );
							con_StreamWriteHexDWORD( 
								GetConfigDWORD( bus, device, function, 0x10 ) );
							con_StreamWriteChar( ' ' );
							con_StreamWriteHexDWORD( 
								GetConfigDWORD( bus, device, function, 0x14 ) );
							con_StreamWriteChar( ' ' );
							con_StreamWriteHexDWORD( 
								GetConfigDWORD( bus, device, function, 0x18 ) );
							con_StreamWriteChar( ' ' );
							con_StreamWriteHexDWORD( 
								GetConfigDWORD( bus, device, function, 0x1C ) );
							con_StreamWriteChar( ' ' );
							con_StreamWriteChar( '\n' );
							// Also squirrel away the first one
							global_NE2000Addr = GetConfigDWORD( bus, device, function, 0x10 );
							// Mask off the bottom bits.
							global_NE2000Addr = global_NE2000Addr & 0xFFFC;
						}
					}
				}
			}
		}
	}
}
