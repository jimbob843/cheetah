OUTPUT_FORMAT("binary")
ENTRY(EntryPoint)
INPUT(obj\startup.o)
INPUT(obj\main.o)
INPUT(obj\console.o)
INPUT(obj\sched.o)
INPUT(obj\keyboard.o)
INPUT(obj\klib.o)
INPUT(obj\mouse.o)
INPUT(obj\floppy.o)
INPUT(obj\pci.o)
INPUT(obj\ne2000.o)
INPUT(obj\ip.o)
INPUT(obj\arp.o)
INPUT(obj\icmp.o)
INPUT(obj\udp.o)
INPUT(obj\vga.o)
INPUT(obj\ide.o)
SECTIONS
{
  .text  0x9000 : {
    *(.text)
  }
  .data  : {
    *(.data)
  }
  .bss  :
  { 					
    *(.bss)
  }
}
