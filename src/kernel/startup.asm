;
; Cheetah v1.0  -  Copyright Marlet Limited 2006
;
; File:				startup.asm
; Description:		First part of the second stage. Start of the nitty
;					gritty of getting things up and running.
; Author:			James Smith
; Created:			14-Aug-2006
; Last Modified:	09-May-2007
;

section .text
[BITS 32]			; Protected mode
global EntryPoint
global _SET_INTERRUPT_MASK
global _GET_INTERRUPT_MASK
global _ENABLE_IRQ
global _DISABLE_IRQ
global _ADD_TASK
global _CALL_TSS
global _STOP_PROCESSOR
global _HALT_PROCESSOR
global _IDLE_LOOP
global _SCHED_LOOP
global _CLEAR_BUSY_BIT
global _SET_BUSY_BIT
global _SET_SCHEDULER_BACKLINK
global _END_OF_INT
global _RAISE_INT0
global _DISABLE_INTERRUPTS
global _ENABLE_INTERRUPTS
global _INPORT_BYTE
global _OUTPORT_BYTE
global _INPORT_WORD
global _OUTPORT_WORD
global _INPORT_DWORD
global _OUTPORT_DWORD
global _MEMCPY
global _MEMCLR
global Exception13
extern _kernel_main
extern _GPFExceptionHandler
extern _GenericIRQHandler

;
; MEMORY MAP
;
; 0010 0000  STACK (64k)
; 0011 0000  GDT 8192 entries (64k) 
; 0012 0000  IDT 256 entries (4k)
; 0012 1000  TSS of kernel (12k)
; 0012 4000  kernel heap (76k)
; 0014 0000  TSS of first user process (12k)
; 0014 3000
;


;
; EntryPoint - beginning of kernel startup code
;
EntryPoint:
	; Make sure that the segment registers are setup before we do
	; anything else. Uses GDT setup in boot floppy code.
	; CS has already been set to CODE (0x08) segment
	mov ax, 0x10			; Set to DATA (0x10) segment
	mov ds, ax 
	mov es, ax 
	mov fs, ax 
	mov gs, ax 
	mov ss, ax 
	mov esp, 0x0010FFFC		; Set stack space: 0x00100000->0x00110000
							; DWORD aligned.

	; Stop the floppy motor.
	mov dx, 0x3F2
	mov al, 0x0C
	out dx, al

	call InitGDT
	call InitPIC
	call InitIDT
	call InitPIT

	call StartKernelTask

	sti						; Enabled interrupts
	call _kernel_main		; Call the OS

	; OS is now running, wait for an interrupt.
waitloop:
	hlt				; Stop here, but will continue on return from interrupt
	jmp waitloop


;
; Init GDT
;  Creates a new bigger GDT so we can fit all the tasks and stuff in it
;
InitGDT:
	; Init to all zeros
	xor eax, eax
	mov edi, [gdt_addr]		; Locate GDT at a fixed address
	mov ecx, 0x4000			; 0x10000 bytes / 4 => number of dwords
	rep stosd				; Clear memory at new location

	; Copy GDT template to its new home
	mov esi, gdt_start		; Set source
	mov edi, [gdt_addr]		; Set dest
	mov ecx, 0x08			; 8 DWORDs
	rep movsd				; Move DS:ESI to ES:EDI for ECX dwords

	lgdt [gdt_pointer]		; Load the address of the new GDT

	; NB. The segment registers will need reloading here if the DATA
	;     segment setup in the boot loader is not present in the new GDT!

	ret


;
; Init 8259 (PIC)
;  Maps IRQ0-7  to interrupts 0x20-0x27
;       IRQ8-15 to interrupts 0x28-0x2F
; Interrupts 0x00-0x1F are system interrupts, 0x00-0x10 are currently used exceptions
;  the higher ones are reserved.
;
InitPIC:
	push eax

	mov al, 0x11		; ICW1 - Init command + ICW4 is required
	out 0x20, al		; PIC1 init
	mov al, 0x20		; ICW2 - Set vector table offset
	out 0x21, al		; Start IRQ0 at interrupt 0x20
	mov al, 0x04		; ICW3 - IRQ2 is connected to slave
	out 0x21, al		; IRQ2 connect to slave
	mov al, 0x01		; ICW4 - 8086 mode
	out 0x21, al		; 80x86, flags

	mov al, 0x11		; ICW1 - Init command + ICW4 is required
	out 0xA0, al		; PIC2 init
	mov al, 0x28		; ICW2 - Set vector table offset
	out 0xA1, al		; Start IRQ8 at interrupt 0x28
	mov al, 0x02		; ICW3 - Slave 2
	out 0xA1, al		; 
	mov al, 0x01		; ICW4 - 8086 mode
	out 0xA1, al		; 80x86, flags

	mov al, 0xFF		; All interrupts off while we setup
	out 0x21, al
	mov al, 0xFF
	out 0xA1, al
	
	pop eax
	ret


;
; InitIDT
;  Creates the IDT at 0x00120000
;
InitIDT:
	; Clear IDT table
	xor eax, eax
	mov ecx, 0x200		; 256 entries, 8 bytes per entry = 0x200 DWORDs
	mov edi, 0x00120000
	rep stosd
	
	;  (exception 0)
	mov ebx, 0x00120000			; Address of EXP0 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 1)
	mov ebx, 0x00120008			; Address of EXP1 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 2)
	mov ebx, 0x00120010			; Address of EXP2 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 3)
	mov ebx, 0x00120018			; Address of EXP3 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 4)
	mov ebx, 0x00120020			; Address of EXP4 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 5)
	mov ebx, 0x00120028			; Address of EXP5 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 6)
	mov ebx, 0x00120030			; Address of EXP6 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 7)
	mov ebx, 0x00120038			; Address of EXP7 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 8)
	mov ebx, 0x00120040			; Address of EXP8 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 9)
	mov ebx, 0x00120048			; Address of EXP9 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	; Set the TSS ex (exception 10)
	mov ebx, 0x00120050			; Address of EXP10 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	; Set the Segment ex (exception 11)
	mov ebx, 0x00120058			; Address of EXP11 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 12)
	mov ebx, 0x00120060			; Address of EXP12 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	; Set the GPF (exception 13)
	mov ebx, 0x00120068			; Address of EXP13 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 14)
	mov ebx, 0x00120070			; Address of EXP14 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 15)
	mov ebx, 0x00120078			; Address of EXP15 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 16)
	mov ebx, 0x00120080			; Address of EXP16 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 17)
	mov ebx, 0x00120088			; Address of EXP17 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	;  (exception 18)
	mov ebx, 0x00120090			; Address of EXP18 IDT entry
	mov eax, Exception13		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax,16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	; Attach timer to scheduler task gate
	mov ebx, 0x00120100			; Address of IRQ0 IDT entry
	mov word [ebx+0], 0x0000	; Unused
	mov word [ebx+2], 0x0018	; TSS Descriptor of kernel task
	mov word [ebx+4], 0x8500	; Present, DPL=0, Task Gate
	mov word [ebx+6], 0x0000	; Unused

	; Set keyboard interrupt handler
	mov ebx, 0x00120108			; Address of IRQ1 IDT entry
	mov eax, GenericInterrupt1		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax, 16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits
	
	; Set floppy interrupt handler
	mov ebx, 0x00120130			; Address of IRQ6 IDT entry
	mov eax, GenericInterrupt6		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax, 16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits
	
	; Set ne2000 interrupt handler
	mov ebx, 0x00120148			; Address of IRQ9 IDT entry
	mov eax, GenericInterrupt9		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax, 16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits
	
	; Set PS/2 mouse interrupt handler
	mov ebx, 0x00120160			; Address of IRQ12 IDT entry
	mov eax, GenericInterrupt12		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax, 16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits

	; Set length/pointer values and load IDT
	mov ebx, idt_pointer			; Set length/pointer address
	mov word [ebx], 512				; 512 bytes long
	mov dword [ebx+2], 0x00120000	; IDT base at 0x00120000
	lidt [idt_pointer]

	ret

;
; InitPIT (8253/2854)
;	Sets up the timer to fire IRQ0 every 50ms or so.
;
InitPIT:
	mov al, 00110100b		; channel 0, lobyte/hibyte, rate generator
	out 0x43, al			; send to command write port
	
;	mov ax, 0xFFFF			; set rate to 65535 = 54.9254ms
	mov ax, 0x1000			; set rate to  4096 =  0.2441ms
	out 0x40, al			; send lowbyte to Channel 0 data port
	mov al, ah
	out 0x40, al			; send hibyte to Channel 0 data port
	
	ret


;
; IRQ0 Handler
;
Interrupt0:
	push eax

	mov al, 0x60		; Send EOI for IRQ0
	out 0x20, al
	
	pop eax
	iret
	
;
; IRQ1 Handler (Generic)
;
GenericInterrupt1:
	pusha

	push 1
	call _GenericIRQHandler
	pop eax

	; Send a non-specific EOI
	mov al, 0x20
	out 0x20, al
	mov al, 0x20
	out 0xA0, al

	popa
	iret
	
;
; IRQ6 Handler (Generic)
;
GenericInterrupt6:
	pusha

	push 6
	call _GenericIRQHandler
	pop eax

	; Send a non-specific EOI
	mov al, 0x20
	out 0x20, al
	mov al, 0x20
	out 0xA0, al

	popa
	iret
	
;
; IRQ9 Handler (Generic)
;
GenericInterrupt9:
	pusha

	push 9
	call _GenericIRQHandler
	pop eax

	; Send a non-specific EOI
	mov al, 0x20
	out 0x20, al
	mov al, 0x20
	out 0xA0, al

	popa
	iret
	
;
; IRQ12 Handler (Generic)
;
GenericInterrupt12:
	pusha

	push 12
	call _GenericIRQHandler
	pop eax

	; Send a non-specific EOI
	mov al, 0x20
	out 0x20, al
	mov al, 0x20
	out 0xA0, al

	popa
	iret
	
	
;
; Exception 13 Routine (GPF)
;
Exception13:
	cli
	call _GPFExceptionHandler
	hlt


;
; StartKernelTask
;
;  Allocates the first TSS with all the right bits for the kernel
;  system process (process 0) and sets it running.
;  This identifies the currently running code with the TSS.
;  TSS + 8K IO Bitmap = 0x2068 bytes, plus a few extra to deal with overruns
;   when reading from the IO Bitmap.
;
; TODO: Do we need the IO Bitmap?
;
StartKernelTask:
	mov edi, 0x121000	; Locate TSS at a fixed address

	; Init to all zeros
	xor eax, eax
	mov ecx, 0x34		; 0x68 bytes / 2 => number of words
	rep stosw

	mov edi, 0x121000
	mov word [edi+0x66], 0x68	; Set offset of IO Bitmap from start of TSS

	; Load the task register with the kernel TSS selector from the new GDT.
	; This initialises the current task, but does not cause a task switch.
	mov ax, 0x0018		; TSS Selector (index 3 + TI(0), RPL(0))
	ltr ax	

	ret


;
; STOP_PROCESSOR
;
_STOP_PROCESSOR:
	cli
	hlt
	
;
; HALT_PROCESSOR
;
_HALT_PROCESSOR:
	hlt
	ret

;
; IDLE_LOOP
;
_IDLE_LOOP:
	hlt
	jmp _IDLE_LOOP


;
; Define interrupt mask
;  ESP+4 = int mask
;
_SET_INTERRUPT_MASK:
	push eax
	
	mov eax, [esp+8]	; Get the mask value
	
	out 0x21, al		; Set interrupt mask (bottom 8 bits)
	shr eax, 8			; Move to the next 8 bits
	out 0xA1, al		; Set interrupt mask (top 8 bits)
	
	pop eax
	ret
	
;
; Returns the interrupt mask in EAX
;
_GET_INTERRUPT_MASK:
	xor eax, eax
	in al, 0xA1
	shl eax, 8
	in al, 0x21
	ret

;
; Enable a specific interrupt at the PIC
;  [input] ESP+4 = irq
;
_ENABLE_IRQ:
	cli
	push eax
	push ebx
	mov ebx, [esp+12]	; Get the irq number (0-15)
	
	; Get the current mask setting
	in al, 0xA1
	shl eax, 8
	in al, 0x21
	
	btr eax, ebx		; Clear the bit in the mask
	
	; Put the mask back
	out 0x21, al
	shr eax, 8
	out 0xA1, al
	
	pop ebx
	pop eax
	sti
	ret
	
	
_DISABLE_IRQ2:
	xor eax, eax
	in al, 0xA1
	shl eax, 8
	in al, 0x21

	bts eax, 12

	out 0x21, al		; Set interrupt mask (bottom 8 bits)
	shr eax, 8			; Move to the next 8 bits
	out 0xA1, al		; Set interrupt mask (top 8 bits)
	ret

;
; Disable a specific interrupt at the PIC
;  [input] ESP+4 = irq
;
_DISABLE_IRQ:
	cli
	push eax
	push ebx
	
	xor eax, eax
	mov ebx, [esp+12]	; Get the irq number (0-15)
	
	; Get the current mask setting
	in al, 0xA1			; Get high byte
	shl eax, 8
	in al, 0x21			; Get low byte
	
	bts eax, ebx		; Set the bit in the mask
	
	; Put the mask back
	out 0x21, al		; Set low byte
	shr eax, 8
	out 0xA1, al		; Set high byte
	
	pop ebx
	pop eax
	sti
	ret
	

;
; Used at the end of the scheduler
;
_END_OF_INT:
	cli				; 
	push eax
	mov al, 0x60	; generate end of interrupt (IRQ0 Specific)
	out 0x20, al
	pop eax

	iret            ; Peform the task switch
					
	cli				; Prevent further interrupts interfering with the scheduler
	ret				; Return to the main loop when we get back here!
	

;
; Add a new task to the GDT
;  ESP+4  = Address of new TSS
;
_ADD_TASK
	mov eax, [esp+4]	; Get the TSS base address
	push ebx
	push ecx
	push edx
	
	xor ebx, ebx
	xor ecx, ecx
	xor edx, edx
	
	; Update the GDT values
	mov bx, [gdt_pointer]		; Get length of GDT = new descriptor
	mov dx, bx					; Save for return value
	add ebx, [gdt_addr]			; Add the start address
	
	mov cx, dx					; Now work out the new length
	add ecx, 0x08				; Add length of new entry
	mov word [gdt_pointer], cx	; Write back the new value
	
	; Initialise the contents of the new entry
	mov word [ebx+0], 0x0098	; 0x0098 bytes long  (0x30 of OS specific)
	mov word [ebx+2], ax		; Bottom two bytes of address
	shr eax, 16					; Move to next bytes of address
	mov byte [ebx+4], al		; byte 3 of address
	mov byte [ebx+5], 0x89		; Present, DPL=0, System=0 (True), X=32bit, Busy=0 (11101001)
	mov byte [ebx+6], 0x00		; G=1byte, AVL=0
	mov byte [ebx+7], ah		; byte 4 (MSB) of address
	
	lgdt [gdt_pointer]			; Update the GDT registers
	
	mov eax, edx				; Set the return value (TSS desc)
	pop edx
	pop ecx
	pop ebx
	
	ret	

;
; Switch to a task by TSS selector
;  just performs a far call to the selector placed on the stack
;
_CALL_TSS
	push eax
	mov eax, [esp+8]
	mov word [taskptr_selector], ax
	pop eax

	cli
	mov al, 0x00		; All interrupts on
	out 0x21, al
	mov al, 0x00		; All interrupts on
	out 0xA1, al
	sti

	jmp far [taskptr_offset]
	ret

;
; Set the backlink of the scheduler task, so we perform a task switch
;  when we IRET
;  ESP+4 = TSS Descriptor of new task
_SET_SCHEDULER_BACKLINK
	push eax
	mov eax, [esp+8]
	mov word [0x00121000], ax		; Fixed TSS location
	pop eax
	ret

;
; Clears the busy bit of a process
;  ESP+4 = TSS Descriptor of old task
;
_CLEAR_BUSY_BIT
	push eax
	push ebx
	mov eax, [esp+12]				; Move the TSS descriptor into AX
	mov ebx, 0x00110000				; Get address of GDT
	add ebx, eax					; Calculate start of TSS entry
	mov byte al, [ebx+5]
	and al, 11111101b				; Clear the busy bit (bit1)
	mov byte [ebx+5], al
	pop ebx
	pop eax
	ret
	
;
; Sets the busy bit of a process
;  ESP+4 = TSS Descriptor of task
;
_SET_BUSY_BIT
	push eax
	push ebx
	mov eax, [esp+12]				; Move the TSS descriptor into AX
	mov ebx, 0x00110000				; Get address of GDT
	add ebx, eax					; Calculate start of TSS entry
	mov byte al, [ebx+5]
	or al, 00000010b				; Set the busy bit (bit1)
	mov byte [ebx+5], al
	pop ebx
	pop eax
	ret
	
;
; Raises an interrupt to switch to the scheduler
;
_RAISE_INT0:
	int 0x20
	ret
	
;
; Disable all interrupts
;
_DISABLE_INTERRUPTS:
	cli
	ret
	
;
; Enable all interrupts
_ENABLE_INTERRUPTS:
	sti
	ret

;
; INPORT_BYTE
;  [input]  ESP+4 = port
;  [output] EAX   = value
;
_INPORT_BYTE:
	push edx
	xor eax, eax			; Clear EAX
	xor edx, edx			; Clear EDX

	mov edx, [esp+8]		; Get port from input parameter
	in al, dx				; Get value from port and put in AL

	pop edx
	ret						; Result returned in EAX

;
; OUTPORT_BYTE
;  [input] ESP+4 = port
;  [input] ESP+8 = value
;
_OUTPORT_BYTE:
	push eax
	push edx
	
	mov edx, [esp+12]		; Get the port
	mov eax, [esp+16]		; Get the value
	out dx, al				; Send the value to the port
	
	pop edx
	pop eax
	ret
	
;
; INPORT_WORD
;  [input]  ESP+4 = port
;  [output] EAX   = value
;
_INPORT_WORD:
	push edx
	xor eax, eax			; Clear EAX
	xor edx, edx			; Clear EDX

	mov edx, [esp+8]		; Get port from input parameter
	in ax, dx				; Get value from port and put in AX

	pop edx
	ret						; Result returned in EAX

;
; OUTPORT_WORD
;  [input] ESP+4 = port
;  [input] ESP+8 = value
;
_OUTPORT_WORD:
	push eax
	push edx
	
	mov edx, [esp+12]		; Get the port
	mov eax, [esp+16]		; Get the value
	out dx, ax				; Send the value to the port
	
	pop edx
	pop eax
	ret
	
	
;
; INPORT_DWORD
;  [input]  ESP+4 = port
;  [output] EAX   = value
;
_INPORT_DWORD:
	push edx
	xor eax, eax			; Clear EAX
	xor edx, edx			; Clear EDX

	mov edx, [esp+8]		; Get port from input parameter
	in eax, dx				; Get value from port and put in EAX

	pop edx
	ret						; Result returned in EAX

;
; OUTPORT_DWORD
;  [input] ESP+4 = port
;  [input] ESP+8 = value
;
_OUTPORT_DWORD:
	push eax
	push edx
	
	mov edx, [esp+12]		; Get the port
	mov eax, [esp+16]		; Get the value
	out dx, eax				; Send the value to the port
	
	pop edx
	pop eax
	ret
	
;
; MEMCPY
;  [input] ESP+4  = src
;  [input] ESP+8  = dest
;  [input] ESP+12 = bytes
;
_MEMCPY:
	push esi
	push edi
	push ecx
	
	; Copy the main block using DWORDs
	mov esi, [esp+16]		; Load source address
	mov edi, [esp+20]		; Load destination address
	mov ecx, [esp+24]		; Load number of bytes to copy
	shr ecx, 2				; Convert to whole DWORDS
	rep movsd
	
	; Copy any odd bytes lefts using BYTEs
	mov ecx, [esp+24]		; Get the number of bytes again
	and ecx, 0x03			; Mask off bottom 2 bits
	rep movsb
	
	pop ecx
	pop edi
	pop esi
	ret
	
;
; MEMCLR
;  [input] ESP+4  = addr
;  [input] ESP+8  = bytes
;
_MEMCLR:
	push eax
	push edi
	push ecx
	
	; Set the main block using DWORDs
	mov edi, [esp+16]		; Load destination address
	mov ecx, [esp+20]		; Load number of bytes to copy
	xor eax, eax			; Set EAX = 0
	shr ecx, 2				; Convert to whole DWORDS
	rep stosd
	
	; Copy any odd bytes lefts using BYTEs
	mov ecx, [esp+20]		; Get the number of bytes again
	and ecx, 0x03			; Mask off bottom 2 bits
	rep movsb
	
	pop ecx
	pop edi
	pop eax
	ret

	
;
; IDT
;
idt_pointer:
	dw 0		; Length of IDT (bytes)
	dd 0		; IDT base address

;
; GDT
;
gdt_start:
	dw 0x0000	; NULL selector
	dw 0x0000
	dw 0x0000
	dw 0x0000

	dw 0xFFFF	; CODE selector, Base=0000, Len=4GB
	dw 0x0000	; Base
	dw 0x9A00	; Present, DPL=0, DT=App, CODE, Execute/Read, Non-conforming
	dw 0x00CF	; G=4K, D=32bit, Seg high nibble = 0xF

	dw 0xFFFF	; DATA selector, Base=0000, Len=4GB
	dw 0x0000	; Base
	dw 0x9200	; Present, DPL=0, DT=App, DATA, Read/Write, Expand-up
	dw 0x00CF	; G=4K, D=32bit, Seg high nibble = 0xF

	dw 0x0068	; SYSTEM TSS 0x0068 bytes long
	dw 0x1000	; Base address = 0x121000
	dw 0x8912	; Present, DPL=0, System=0 (True), X=32bit, Busy=0 (10001001) + 3rd byte of address
	dw 0x0000	; G=1byte, AVL=0, + upper nibble of size + more address (00000000) 

gdt_pointer:
	dw 0x0020	; Calculate length of GDT (max 8192 entries)
gdt_addr:
	dd 0x00110000	; Fixed GDT location

;
; Task pointer for switching tasks
;
taskptr_offset:
	dd 0	; offset
taskptr_selector:
	dw 0	; selector
	