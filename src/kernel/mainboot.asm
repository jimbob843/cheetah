;
; Cheetah v1.0  -  Copyright Marlet Limited 2006
;
; File:				bootstrap.asm
; Description:		This is the main entry point to everything. This
;					code builds the first 512 bytes bootblock on the
;					boot floppy. It's main objective is to locate the
;					stage two boot code and load it into memory.
; Author:			James Smith
; Created:			14-Aug-2006
; Last Modified:	01-May-2006
;

[BITS 16]		; Real mode
[ORG 0x7C00]	; Standard boot block load address

;
; MEMORY MAP
;
; 0000:0000 Interrupt Vector Table (IVT)
; 0000:0400 BIOS data area
; 0000:0500 Cheetah FAT storage area
; 0000:4500 FREE
; 0000:5000 Cheetah data area
; 0000:7C00	Bootblock
; 0000:7E00 FREE
; 0000:8000 Cheetah boot stack
; 0000:9000 Stage Two
; A000:0000 VGA Video RAM
; C000:0000 BIOS ROM
; FFFF:FFFF
; 

;
; Boot Parameter Block (BFB) - Needed for the disk to be DOS compatible.
;

	jmp mainstart		; Must be 3 bytes before vendor ident
	
					db 'MSWIN4.1'	; Vendor ident
SectorSize			dw 512			; SS - Sector size in bytes
SectorsPerCluster	db 1			; AU - Sectors per cluster
NumberOfRS			dw 1			; RS - Reserved sector count
NumberOfFATs		db 2			; NF - Number of FATs on this disk
EntriesPerDir		dw 224			; DS - Directory size (number of files)
					dw 2880			; TS - Total number of sectors
					db 0xF0			; MD - Media descriptor (1.44MB floppy)
SectorsPerFAT		dw 9			; FS - Sectors per FAT
SectorsPerTrack		dw 18			; ST - Sectors per track
NumHeads			dw 2			; NH - Number of disk heads
					dd 0			; HS - Number of hidden sectors
					dd 0			; LS - Large sector count
					db 0			; Drive number (0x00 = floppy, 0x80 = HD)
					db 0			; Reserved
					db 0x29			; BootSig (0x29 indicates that the following three fields are present)
					dd 0xFFFFFFFF	; VolumeID (date and time)
					db 'XXXXXXXXXXX' ; Volume label (11 bytes)
					db 'FAT12   '	; SystemID (8 bytes)
					
					
KernelBin			db 'KERNEL  BIN'	; 8.3 filename of the second stage
NoSecondStageMsg	db 'No kernel.bin found!',13,10,0
SecondStageFoundMsg	db 'KERNEL.BIN Loaded ok.',13,10,0					
A20ErrorMsg			db 'A20 error',13,10,0

;
; Main Code Start
;
mainstart:
	mov ax, 0x0000
	mov ds, ax		; Reset DS
	mov ss, ax		; Reset SS
	mov sp, 0x9000	; Allocate 0x1000 bytes of stack
	
;
; Use the BIOS INT 15h to find out how much memory there is.
; Use AX=E801h => AX=1-16MB(1k),BX=16MB+(64k),CX=1-16MB(1k),DX=16MB+(64k)
;
; eg: AX=3C00 => 15360 x  1k = 15MB (and then +1MB)
;     BX=0100 => 256   x 64k = 16MB
;  gives 32MB in total.
;
	mov ax, 0xE801
	int 0x15
	; Save values
	mov [0x5000],ax
	mov [0x5002],bx
	mov [0x5004],cx
	mov [0x5006],dx

	call LoadStageTwo
	call EnableA20
	call CheckA20
	call EnterPMode

	cli
	hlt		; Shouldn't get here. Stop processor.


;
; Stage Two Boot Load
;   - Load the FAT and FAT12 directory from disk to 0x00500 (sectors 1-33)
;   - Locate the KERNEL.BIN in the root dir
;   - Load the file at 0x09000. (Gives 604k of space from 0x09000->0xA0000)
;
; NB: To save time and space, this code assumes a normal 1.44MB floppy
;     with 512 bytes per sector.
;
LoadStageTwo:
	; Setup load directory, start sector, and number of sectors to load.
	mov bx, 0x0000
	mov es, bx
	mov bx, 0x0500		; Load data at 0x0500
	mov cx, 32			; Read 32 sectors
	mov ax, 1			; Start at sector 1 (LBA start at 0)
	
.nextsector
	call LoadLBA		; Get one sector
	inc ax				; Move to next sector
	add bx, 0x200		; Move dest addr by number of bytes loaded
	dec cx				; Decrement sector counter
	jnz .nextsector
	
	; All sectors loaded now, scan the directory table for the file.
	mov bx, 0x2900		; Hard-coded start of FAT12 directory table.
	mov si, KernelBin	; String to search for
	mov di, bx			; Start address of search
	mov cx, 224			; File entries per dir.
	
.nextentry
	call StringCompare
	cmp dx, 1
	jz .foundentry
	
	; No entry found, move to next entry
	add di, 32			; 32 bytes per entry
	loop .nextentry
	
	; String not found in any of the entries, print message and stop
	mov si, NoSecondStageMsg
	call WriteStr
	
	cli
	hlt		; STOP
	
.foundentry
	; BX currently points to the directory entry we want
	mov ax, [bx+26]		; Get the first cluster number
						; Assume SectorsPerCluster = 1
						
	; Now load the file into memory at 0x09000
	mov bx, 0x9000
.loadnextsector
	push ax
	add ax, 31			; First cluster is (32-1) sectors from the start of disk
	call LoadLBA
	
	pop ax
	call GetSector		; Get the next cluster from the FAT
	add bx, 0x200		; Move dest addr by number of bytes read
	
	cmp ax, 0x0FFF		; Check for end of FAT chain
	jnz .loadnextsector
	
	ret					; Second stage now loaded
	
;
; StringCompare
;
; SI - string 1
; DI - string 2
; returns DX : 0 = no match, 1 = match
;
StringCompare:
	push cx
	mov cx, 11		; Assume length of the filename we are looking for
	xor dx, dx		; Init DX to not found
	cld				; Set direction to up
.compareloop2
	cmpsb			; Compare [SI] with [DI]
	jnz .exitcompare	; ZF set if character matches
	loop .compareloop2	; do the next one
 
	inc dx			; Match found, set return = 1
.exitcompare
	pop cx
	ret


;
; WriteStr
;
; [input] si = addr of string
;
WriteStr:
	mov ah,0x0E	; Function (teletype)
	mov bh,0x00	; Page
	mov bl,0x07	; White text

.nextchar
	lodsb		; Loads [SI] into AL and increases SI by one
	or al, al	; check al = 0
	jz .return
	int 0x10	; BIOS print char
	jmp .nextchar

.return
	ret

;
; LoadLBA
;   AX=LBA sector number
;   BX=Dest addr
;
LoadLBA:
	push dx
	push cx
	push ax		; Save trashed registers
	push bx		; Save dest addr for later

	xor dx, dx
	mov bx, 18		; Sectors per track
	div bx			; ax/bx -> ax,dx
	inc dx
	mov cx, dx		; Put sector count into CL

	xor dx, dx
	mov bx, 2		; Number of Heads
	div bx			; AL=Cylinder, DL=Head, CL=Sector

 ; Now do the load
	mov ch, al		; Put the cylinder (track) into CH
	shl dx, 8		; Shift head into DH
	mov dl, 0x00	; Set drive to A:
	mov ah, 0x02	; BIOS read command
	mov al, 0x01	; One sector
	pop bx			; Get the address to put the data that we saved earlier
	int 0x13		; Do it.
	
	; TODO: Need to check for success here.

	pop ax
	pop cx
	pop dx
	ret

;
; GetSector - Finds the next cluster from the FAT
;  AX - current cluster no., returns next cluster number in AX
;
GetSector:
	push bx
	push cx
	push dx

	mov cx, ax		; save AX for later

	mov dx, 3		; 3 nibbles per FAT entry
	mul dx			; Calculate offset in nibbles
	shr ax, 1		; Convert to bytes  (/2)
	add ax, 0x0500	; Start of FAT
	mov bx, ax
	mov ax, [bx]	; Get both bytes  BC DA => DABC (even), CD AB => ABCD (odd)

	bt cx, 0		; Check to see if CX (input cluster num) is even
	jc .oddFAT
.evenFAT
	and ax, 0x0FFF	; Mask off the bits we don't want => 0ABC
	jmp .sectorexit

.oddFAT
	shr ax, 4		; Move the value (ABCD) down to the right place => 0ABC

.sectorexit
	pop dx
	pop cx
	pop bx
	ret


;
; Enable A20 using standard keyboard controller method
;
EnableA20:
    call KeyboardWait
    
    mov al, 0xD0        ; "Read Output Port" command
    out 0x64, al

	; Wait for the data to appear in the output buffer    
    call KeyboardWaitData
    
    in al, 0x60         ; Get the value
    mov bl, al          ; Save for later
    
    call KeyboardWait
    
    mov al, 0xD1        ; "Write Output Port" command
    out 0x64, al
    
    call KeyboardWait
    
    mov al, bl
    or al, 2            ; Set bit1 to enable A20
    out 0x60, al

    call KeyboardWait
    
	ret

;
; CheckA20
;
CheckA20:
    call KeyboardWait
    
    mov al, 0xD0        ; "Read Output Port" command
    out 0x64, al
    
    call KeyboardWaitData
    
    xor ax, ax
    in al, 0x60         ; Get the value
    bt ax, 1			; Check bit1
    jnc A20Error		; A20 not enabled

	ret


A20Error:
	mov si, A20ErrorMsg
	call WriteStr

	cli
	hlt		; STOP


;
; KeyboardWait - wait for keyboard controller to be ready
;
KeyboardWait:
	push ax

.keepwaiting
	xor ax, ax
	in al, 0x64
	bt ax, 1		; Check bit1 of controller status
	jc .keepwaiting
	
	pop ax
	ret

;
; KeyboardWaitData - wait for keyboard controller to be ready
;
KeyboardWaitData:
	push ax

.keepwaiting
	xor ax, ax
	in al, 0x64
	bt ax, 0			; Check bit0 of controller status
	jnc .keepwaiting	; Loop until 1
	
	pop ax
	ret

;
; Enter Protected Mode
;
EnterPMode:
	cli						; Stop interrupts
	lgdt [gdt_pointer]		; Load GDT

	mov eax, cr0
	or al, 1				; Set bit0 of cr0
	mov cr0, eax

	jmp 0x08:0x9000		; Do far jmp to enable pmode
						; CODE segment (0x08)


;
; GDT (This is just a temporary one to switch to PMode)
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

gdt_pointer:
 dw gdt_pointer - gdt_start - 1		; Calculate length of GDT
 dd gdt_start


;
; Bootblock end
;
times 510-($-$$) db 0	; Fill with zeros so that binary output is 512 byte
dw 0xAA55				; Boot loaded signature
