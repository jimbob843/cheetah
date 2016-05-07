/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			sched.c
 * Description:		Kernel process scheduler
 * Author:			James Smith
 * Created:			16-Aug-2006
 * Last Modified:	09-May-2007
 *
 */
 
#include "kernel.h"

void        sch_InitScheduler( void );
ProcessTSS *sch_CreateProcess( void *EntryPoint );
void IdleProcess_EntryPoint( void );
static ProcessTSS *sch_GetNextReadyProcess( ProcessTSS *start );

static ProcessTSS *ProcessHead;	// Head of linked list of processes
static ProcessTSS *ProcessTail;	// Last item in list of processes
static ProcessTSS *IdleProcess;
ProcessTSS *CurrentProcess;


void sch_InitScheduler( void ) {
		
	// Initialises the process scheduler and sets it running.

	// Setup the process list
	ProcessHead = NULL;
	ProcessTail = NULL;

	// Create the idle process
	IdleProcess = sch_CreateProcess( (void *) IdleProcess_EntryPoint);
	IdleProcess->ProcessStatus = PROCSTATUS_IDLE;
	CurrentProcess = ProcessHead;

	CALL_TSS(CurrentProcess->TSSDescriptor);
}


ProcessTSS *sch_CreateProcess( void *EntryPoint ) {
	// Creates a new process by creating a TSS and adding an entry 
	//  in the GDT.

	// Allocate some memory for the new TSS
	ProcessTSS *newtss = (ProcessTSS *)kmalloc(sizeof(ProcessTSS));

	// Populate with values
	newtss->ES = 0x10;
	newtss->CS = 0x08;
	newtss->SS = 0x10;
	newtss->DS = 0x10;
	newtss->FS = 0x10;
	newtss->GS = 0x10;
	newtss->IOBaseOffset = 0x0098;		// Set the length of the TSS
	newtss->EIP = (DWORD)EntryPoint;	// Process entry point
	newtss->EFLAGS = 0x00000200;		// Have IF=1 to enable interrupts

	// Allocate some stack space
	DWORD stack = (DWORD)kmalloc(2048);		// 2k stack space
	newtss->ESP = stack + 2048 - 4;			// Put ESP at the top-4

	newtss->ProcessStatus = PROCSTATUS_READY;
	newtss->Priority = 128;

	// Turn off interrupts while we're changing the task list
	DISABLE_INTERRUPTS();

	// Install the new process in the GDT
	newtss->TSSDescriptor = (WORD) ADD_TASK( newtss );

	// Add the process to the process list
	newtss->PrevTSS = ProcessTail;
	if (ProcessTail != NULL) {
		ProcessTail->NextTSS = newtss;
	}
	ProcessTail = newtss;
	if (ProcessHead == NULL) {
		// This is the first process
		ProcessHead = newtss;
	}

/*
	kprint("TSS Created. ADDR=");
	kprintDWORD(newtss);
	kprint(" TSS=");
	kprintDWORD(newtss->TSSDescriptor);
	kprint(" ESP=");
	kprintDWORD(newtss->ESP);
	kprint("\n");
*/

	// Turn interrupts back on
	ENABLE_INTERRUPTS();

	return newtss;
}


void sch_EndProcess( ProcessTSS *proc ) {
	// Removes the selected process from the process list
	if (proc->PrevTSS != NULL) {
		// Make the previous TSS point to the next one
		proc->PrevTSS->NextTSS = proc->NextTSS;
	} else {
		// This is the first TSS
		ProcessHead = proc->NextTSS;
	}
	if (proc->NextTSS != NULL) {
		// Make the next TSS point to the previous one
		proc->NextTSS->PrevTSS = proc->PrevTSS;
	} else {
		// This is the last TSS
		ProcessTail = proc->PrevTSS;
	}
	
	// TODO: Free TSS struct and stack

	// Select a new task  NB: This code needs to be different between
	//  a system call and a scheduler operation.
	// The TSS is invalid at this point. Is that a problem?
	RAISE_INT0();
}


void sch_ScheduleInterrupt( void ) {
	// The timer has told us to wake up.
	// Look at the process list, and switch tasks if necessary

	// Put the current process back to READY.
	if (CurrentProcess == NULL) {
		// No current process active
	} else {
		if (CurrentProcess->ProcessStatus == PROCSTATUS_RUNNING) {
			// Stop us running to allow other tasks to run
			CurrentProcess->ProcessStatus = PROCSTATUS_READY;
		}
	}

	ProcessTSS *next = sch_GetNextReadyProcess( CurrentProcess );
	if (next == NULL) {
		// Couldn't find another ready process
		// Should never happen as the idle process should always be ready
		if (CurrentProcess != NULL) {
			CLEAR_BUSY_BIT(CurrentProcess->TSSDescriptor);
		}
		CurrentProcess = IdleProcess;
//		next->ProcessStatus = PROCSTATUS_RUNNING;
		SET_BUSY_BIT(CurrentProcess->TSSDescriptor);
		SET_SCHEDULER_BACKLINK(CurrentProcess->TSSDescriptor);
	} else {
		if (next == CurrentProcess) {
			// Still the same process, no task switch necessary
		} else {
			// Switch to a new process
			// Set the back link of the scheduler task to go
			//  "back" to the new task when we IRET
			// Also need to clear the busy bit of the current task
			if (CurrentProcess != NULL) {
				CLEAR_BUSY_BIT(CurrentProcess->TSSDescriptor);
			}
			CurrentProcess = next;
			next->ProcessStatus = PROCSTATUS_RUNNING;
			SET_BUSY_BIT(next->TSSDescriptor);
			SET_SCHEDULER_BACKLINK(next->TSSDescriptor);
		}
	}

}


static ProcessTSS *sch_GetNextReadyProcess( ProcessTSS *start ) {
	// Returns the next ready process. Assumes that at least one
	//  process will always be ready - the idle process.
	
	ProcessTSS *current = start;
	ProcessTSS *nextready = NULL;

	do {
		current = current->NextTSS;
		if (current == NULL) {
			current = ProcessHead;		// Back to the head of the list
		}
		if (current->ProcessStatus == PROCSTATUS_READY) {
			nextready = current;
		}
	} while ((current != start) && (nextready == NULL));

	return nextready;
}


void sch_ProcessWait( Event *w ) {
	// Set the process into a waiting state
	CurrentProcess->ProcessStatus = PROCSTATUS_WAITING;
	CurrentProcess->WaitingEvent = w;

	// Cause a schedule event to switch to another process
	RAISE_INT0();
}

void sch_ProcessNotify( Event *w ) {
	ProcessTSS *c = ProcessHead;

	// Find all the processes waiting for this event, and mark them ready.
	while (c != NULL) {
		if ((c->WaitingEvent == w) && (c->ProcessStatus == PROCSTATUS_WAITING)) {
			// Wake up the process
			c->ProcessStatus = PROCSTATUS_READY;
		}
		c = c->NextTSS;
	}
}

void sch_DumpProcessList( void ) {
	ProcessTSS *c = (ProcessTSS*)0x121000;

	while (c != NULL) {
		kprint("TSS: ");
		kprintDWORD(c->TSSDescriptor);
		kprint(" BackLink: ");
		kprintDWORD(c->Link);
		kprint(" EIP: ");
		kprintDWORD(c->EIP);
		kprint(" ESP: ");
		kprintDWORD(c->ESP);
		kprint(" Status: ");
		kprintDWORD(c->ProcessStatus);
		kprint("\n");

		if (c == (ProcessTSS *)0x121000) {
			c = ProcessHead;
		} else {
			c = c->NextTSS;
		}
	}
}

void IdleProcess_EntryPoint( void ) {
	// The idle process. Just halt, and wait for interrupts
	IDLE_LOOP();
}


void sch_DumpGDT( void ) {
	int i = 0;
	DWORD *addr = (DWORD *)0x00110000;
	for (i=0; i<7; i++) {
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint("\n");
	}
}

void sch_DumpIDT( void ) {
	int i = 0;
	DWORD *addr = (DWORD *)0x00120000;
	for (i=0; i<20; i++) {
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint("\n");
	}
}

void sch_DumpKernelTSS( void ) {
	int i = 0;
	DWORD *addr = (DWORD *)0x00121000;
	for (i=0; i<7; i++) {
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint("\n");
	}
	addr = (DWORD *)0x00124000;
	for (i=0; i<7; i++) {
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint(" ");
		kprintDWORD(*addr++);
		kprint("\n");
	}
}


void sch_MainLoop( void ) {
	while (1) {
		sch_ScheduleInterrupt();
		// Perform end of interrupt and iret
		END_OF_INT();
	}
}


