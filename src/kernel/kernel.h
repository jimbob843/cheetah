/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			kernel.h
 * Description:		Main kernel header file (Included in everything)
 * Author:			James Smith
 * Created:			14-Aug-2006
 * Last Modified:	12-Sep-2006
 *
 */


#ifndef __KERNEL_H__
#define __KERNEL_H__

#include "types.h"

#define kprint(x) con_StreamWriteString(x)
#define kprintDWORD(x) con_StreamWriteHexDWORD(x)
extern void *kmalloc( DWORD bytes );

void OUTPORT_BYTE( WORD port, BYTE value );
BYTE INPORT_BYTE( WORD port );

/* Information about the system hardware */
struct _SystemInformation {
	int AvailableMemory;	/* Number of 64k blocks of memory */
};
typedef struct _SystemInformation SystemInformation;


struct _Event {
	BOOL EventStatus;
};
typedef struct _Event Event;


typedef void (*IRQHandlerFunc)(int);

struct _Device {
	BOOL IRQInterrupt;			// Flagged when an interrupt has occurred for this device
	IRQHandlerFunc IRQHandler;	// Pointer to the IRQ handler function
	Event DataReady;
};
typedef struct _Device Device;


struct _DeviceList {
	Device *ThisDevice;
	struct _DeviceList *NextDevice;
};
typedef struct _DeviceList DeviceList;



// Process statuses
#define PROCSTATUS_RUNNING  1
#define PROCSTATUS_READY	2	
#define PROCSTATUS_WAITING	3
#define PROCSTATUS_IDLE		4  // Used to identify the idle process


struct _ProcessTSS {
	WORD  Link;		WORD  Empty1;
	DWORD ESP0;
	WORD  SS0;		WORD  Empty2;
	DWORD ESP1;
	WORD  SS1;		WORD  Empty3;
	DWORD ESP2;
	WORD  SS2;		WORD  Empty4;
	DWORD CR3;
	DWORD EIP;
	DWORD EFLAGS;
	DWORD EAX;
	DWORD ECX;
	DWORD EDX;
	DWORD EBX;
	DWORD ESP;
	DWORD EBP;
	DWORD ESI;
	DWORD EDI;
	WORD  ES;		WORD  Empty5;
	WORD  CS;		WORD  Empty6;
	WORD  SS;		WORD  Empty7;
	WORD  DS;		WORD  Empty8;
	WORD  FS;		WORD  Empty9;
	WORD  GS;		WORD  Empty10;
	WORD  LDT;		WORD  Empty11;
	WORD  DebugTrapBit;
	WORD  IOBaseOffset;			// Must be 0x0068+0x0030 for this struct.

	// OS Specific
	WORD ProcessStatus;
	WORD Priority;
	WORD TSSDescriptor;
	WORD Unused;
	struct _ProcessTSS *PrevTSS;
	struct _ProcessTSS *NextTSS;
	Event *WaitingEvent;
	DWORD Unused2;
};
typedef struct _ProcessTSS ProcessTSS;

#endif /* __KERNEL_H__ */
