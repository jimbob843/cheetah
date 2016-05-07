/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			main.c
 * Description:		Kernel entry point
 * Author:			James Smith
 * Created:			14-Aug-2006
 * Last Modified:	26-May-2007
 *
 */

#include "kernel.h"
#include "klib.h"
#include "floppy.h"
#include "ne2000.h"
#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "vga.h"
#include "ide.h"

//void STOP_PROCESSOR( void );
//DWORD GET_INTERRUPT_MASK( void );
//void kernel_main( void );
//void ConsoleProcess_EntryPoint( void );
//void ConsoleProcess2_EntryPoint( void );
void ConsoleProcess3_EntryPoint( void );
void StartupProcess_EntryPoint( void );
void WaitingProcess_EntryPoint( void );
void *kmalloc( DWORD bytes );
extern ProcessTSS *CurrentProcess;
ProcessTSS *sch_CreateProcess( void *EntryPoint );
//void sch_DumpGDT( void );
//void sch_DumpKernelTSS( void );
void ReportAvailableMemory( void ) ;

void mem_Initialise( void ) {
	ReportAvailableMemory();
}


void ConsoleProcess2_EntryPoint( void ) {
	unsigned char c = '\0';

	while (1) {
			unsigned char *screen = (unsigned char *)0xB8006;
			*screen = c++;
			*(screen+1) = 0x57;
	}
}


void kernel_main( void ) {

	con_Reset();
	con_StreamWriteString("Welcome to Cheetah!\n\n");

	mem_Initialise();

	sch_InitScheduler();

	// Create the kernel startup task. This is responsible for performing
	//  all the driver initialisation etc....
	ProcessTSS *startup = sch_CreateProcess( (void *)StartupProcess_EntryPoint );
//	ProcessTSS *waiting = sch_CreateProcess( (void *)WaitingProcess_EntryPoint );

	sch_MainLoop();

	// If we get here then the OS is no longer processing
	//  scheduling interrupts, so STOP.
	STOP_PROCESSOR();
}


void GPFExceptionHandler( DWORD errorcode, DWORD addr, DWORD cs, 
							DWORD eflags ) {
	kprint("\nGeneral Protection Fault: STOPPING\n");
	kprint("ERROR=");
	kprintDWORD(errorcode);
	kprint(" ADDR=");
	kprintDWORD(addr);
	kprint(" CS=");
	kprintDWORD(cs);
	kprint(" EFLAGS=");
	kprintDWORD(eflags);
	kprint("\n");
	sch_DumpGDT();
	STOP_PROCESSOR();
}


void GenericIRQHandler( BYTE irq ) {

	switch( irq ) {
		case 1:
			key_IRQHandler();
			break;
		case 6:
			flp_IRQHandler();
			break;
		case 9:
			net_IRQHandler();
			break;
		case 12:
			mse_IRQHandler();
			break;
	}

	// TODO: Replace with a generic routine that allows IRQs to
	//  be piggy backed on each other, so we can have multiple
	//  devices on each IRQ.
}


static void *CurrentHeapPointer = (void*)0x00124000;	// See startup.asm

void *kmalloc( DWORD bytes )
{
	// Simple alloc that just gives memory that can never be freed!
	void *allocatedmemory = CurrentHeapPointer;

	// Move the heap pointer, rounding to the next DWORD boundary
	if ((bytes % 4) != 0) {
		bytes += 4 - (bytes % 4);
	}
	CurrentHeapPointer += bytes;

	// Clear the new memory to all zeros
//	MEMSETD( allocatedmemory, bytes >> 2, 0 );

	unsigned char *x = (unsigned char *)allocatedmemory;
	int i = 0;
	for (i=0; i<bytes; i++) {
		x[i] = 0x00;
	}

	return allocatedmemory;
}

void kfree( void *addr ) {
	// No freeing yet!
}


void DisplayPrompt( void ) {
	con_StreamWriteString( "CheetahOS> " );

}

#define CMDLINE_BUFFER_SIZE 256

unsigned char *GetCommandLine( void ) {
	// Get characters from the keyboard device and display them
	//  on screen. Also store them in the command line buffer.

	// Get characters until one of them is a return.
	unsigned char c = '\0';
	unsigned char *buffer = (unsigned char *)kmalloc(CMDLINE_BUFFER_SIZE+1);
	int i = 0;

	while (c != 10) {
		c = getc();
		if (c == 8) {
			if (i > 0) {
				con_StreamBackspaceChar();
				buffer[i--] = 0;
			}
		} else {
			if (i == CMDLINE_BUFFER_SIZE) {
				// Buffer full, do nothing, but
				//  make sure we still write a return
				if (c == 10) {
					con_StreamWriteChar( c );
				}
			} else {
				buffer[i++] = c;
				con_StreamWriteChar( c );
			}
		}
	}

	buffer[--i] = '\0';		// Put the terminator on the end

	return buffer;
}


void ProcessCommandLine( unsigned char *cmdline ) {
	if (strcmp((char*)cmdline,"dumpgdt") == 0) {
		sch_DumpGDT();
	}
	if (strcmp((char*)cmdline,"reinitmouse") == 0) {
		mse_InitDevice();
	}
	if (strcmp((char*)cmdline,"readblock") == 0) {
		BYTE *buffer = kmalloc( 512 );
		// Drive, Head, Cylinder, Sector, Buffer
		flp_ReadSector( 0, 0, 0, 1, buffer );
		con_DumpMemoryBlock( buffer );
	}
	if (strcmp((char*)cmdline,"idereadblock") == 0) {
		BYTE *buffer = kmalloc( 512 );
		// Drive, Head, Cylinder, Sector, Buffer
		ide_ReadSector( 0, 0, 0, 1, buffer );
		con_DumpMemoryBlock( buffer, 16 );
	}

	if (strcmp((char*)cmdline,"pciscan") == 0) {
		pci_DeviceScan();
	}
	if (strcmp((char*)cmdline,"netinit") == 0) {
		net_InitDevice();
	}
	if (strcmp((char*)cmdline,"netread") == 0) {
		BYTE *buffer = kmalloc( 512 );
		WORD length = 512;
		net_ReadPacket( buffer, &length );
		ip_ProcessPacket( buffer, length );
	}
	if (strcmp((char*)cmdline,"netsend") == 0) {
		BYTE *buffer = kmalloc( 512 );
		BYTE *destMAC = kmalloc( 6 );
		WORD length = 64;
		int i =0;
		destMAC[0] = 0xFF;
		destMAC[1] = 0xFF;
		destMAC[2] = 0xFF;
		destMAC[3] = 0xFF;
		destMAC[4] = 0xFF;
		destMAC[5] = 0xFF;

		buffer[6] = 0x00;
		buffer[7] = 0xFF;
		buffer[8] = 0xEE;
		buffer[9] = 0xDD;
		buffer[10] = 0xCC;
		buffer[11] = 0xBB;

		buffer[12] = 0x08;
		buffer[13] = 0x00;
		for (i=14; i<length; i++) {
			buffer[i] = (BYTE) i;
		}
		net_SendPacket( destMAC, buffer, length, FRAMETYPE_ARP );
	}
	if (strcmp((char*)cmdline,"arpreq") == 0) {
		arp_GenerateRequest( 0xC0A80501 );  // 192.168.5.1
	}

	if (strcmp((char*)cmdline,"ping") == 0) {
		// add a test MAC
		con_StreamWriteString( "ping sent to 192.168.5.1\n" );
		icmp_SendEchoRequest( 0xC0A80501, 0x4000 );
	}
	if (strcmp((char*)cmdline,"arp") == 0) {
		arp_DumpARPTable();
		sch_ProcessNotify( (DWORD) 0x5 );
	}

	if (strcmp((char*)cmdline,"help") == 0) {
		con_StreamWriteString( " dumpgdt      - prints the GDT\n" );
		con_StreamWriteString( " reinitmouse  - allows a mouse to be reconnected\n" );
		con_StreamWriteString( " readblock    - test read from floppy device\n" );
		con_StreamWriteString( " idereadblock - test read from hard disk device\n" );
		con_StreamWriteString( " pciscan      - scan for PCI devices (experimental!)\n" );
		con_StreamWriteString( " netinit      - reinitialise the NE2000 device\n" );
		con_StreamWriteString( " netread      - read a packet from the NE2000 device\n" );
		con_StreamWriteString( " netsend      - send a test packet to the NE2000 device\n" );
		con_StreamWriteString( " arpreq       - send an arp request for 192.168.5.1\n" );
		con_StreamWriteString( " arp          - dump the arp table\n" );
	}
#if 0
	if (strcmp((char*)cmdline,"readfile") == 0) {
		FILE *fp = fopen( "test.dat", "r" );
		BYTE *buffer = kmalloc( 512 );
		int bytes_read = fread( fp, buffer, 512 );
		fclose( fp );
	}
#endif
}


void ConsoleProcess3_EntryPoint( void ) {

	unsigned char *cmdline = NULL;

	con_StreamWriteChar( '\n' );

	while (1) {
		DisplayPrompt();
		cmdline = GetCommandLine();
		ProcessCommandLine( cmdline );
	}
}


void StartupProcess_EntryPoint( void ) {

	// First look for the standard devices
	key_InitDevice();
//	mse_InitDevice();
	flp_InitDevice();

	// Now look for any PCI devices
	pci_DeviceScan();

	// TODO: device scan should take care of these
	net_InitDevice();
	ide_InitDevice();

	// Create the main user task (GUI or login)
	sch_CreateProcess( (void *)ConsoleProcess3_EntryPoint );

#if 0
	vga_SetMode( 0 );
	vga_FillScreen( 0x1 );

	vga_DrawPixel( 10, 10, 0x0F );
	vga_DrawPixel( 20, 10, 0x0F );
	vga_DrawPixel( 30, 10, 0x0F );
	vga_DrawPixel( 39, 10, 0x0F );
	vga_DrawPixel( 40, 10, 0x0F );
	int i = 100;
//	for (i=100; i<220; i++) {
//		vga_DrawLine( 10, 10, i, 200, 0x2 );
//	}

	vga_DrawFilledRectangle( 100, 300, 400, 400, 0xF );
	vga_DrawRectangle( 101, 301, 401, 401, 0x0 );
	vga_DrawRectangle( 100, 300, 400, 400, 0x7 );
	vga_DrawFilledRectangle( 102, 302, 379, 319, 0xE );
	vga_DrawLine( 100, 320, 400, 320, 0x7 );
	vga_DrawLine( 101, 321, 399, 321, 0x0 );
	vga_DrawLine( 380, 300, 380, 320, 0x7 );
	vga_DrawLine( 385, 305, 395, 315, 0x0 );
	vga_DrawLine( 385, 315, 395, 305, 0x0 );
#endif

	// Startup complete. End.
	sch_EndProcess( CurrentProcess );

	// TODO: A process will GPF if it exits without calling EndProcess
}

void WaitingProcess_EntryPoint( void ) {

	con_StreamWriteString( "Waiting process has started.\n" );

	sch_ProcessWait( (DWORD) 0x05 );

	con_StreamWriteString( "Waiting process has stopped.\n" );

	sch_EndProcess( CurrentProcess );
}


void ReportAvailableMemory( void ) {

	// Get the memory report from 0x5000 written by mainboot.asm
	WORD *addr = (WORD*)0x5000;
	WORD AX = *addr++;
	WORD BX = *addr++;
	WORD CX = *addr++;
	WORD DX = *addr++;

	DWORD totalkbytes = AX * 1 + 1024;
	totalkbytes += BX * 64;
	DWORD totalmbytes = totalkbytes / 1024;

	con_StreamWriteString( "[DEV] Memory: " );
	con_StreamWriteDecDWORD( totalmbytes );
	con_StreamWriteString( "MB\n" );
}
