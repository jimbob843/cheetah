#
# Cheetah v1.0   Copyright Marlet Limited 2006
#
# File:				makefile
# Description:		Main makefile
# Author:			James Smith
# Created:			14-Aug-2006
# Last Modified:	26-May-2007
#
#  ***** RUN USING NMAKE *****
#

OBJ=obj\startup.o obj\main.o obj\console.o obj\sched.o obj\keyboard.o \
	obj\klib.o obj\mouse.o obj\floppy.o obj\pci.o obj\ne2000.o obj\ip.o \
	obj\arp.o obj\icmp.o obj\udp.o obj\vga.o obj\ide.o

all : cheetah.img

clean :
	del obj\*.o
	del obj\*.bin
	del cheetah.img

cheetah.img : obj\mainboot.bin obj\kernel.bin
	copy obj\kernel.bin Bootdisk\kernel.bin
	bfi -o=kernel.bin -b=obj\mainboot.bin -f=cheetah.img Bootdisk

obj\kernel.bin : $(OBJ)
	ld -E -Map kernel.map -T kernel.ld -o obj\kernel.bin

obj\startup.o : startup.asm
	nasm -f coff startup.asm -o obj\startup.o

obj\mainboot.bin : mainboot.asm
	nasm mainboot.asm -o obj\mainboot.bin

.c{obj\}.o:
	gcc -c $< -o $@
