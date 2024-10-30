OUTPUT_ARCH( "riscv" )


SECTIONS
{
  . = 0x0000; /* 起始地址，可以根据需要调整 */
  .text : {
    user/start.o(_start)
    *(.text*) 
  }
  .rodata : {
    *(.rodata*)
  }
  .data : {
    *(.data*)
  }
  .bss : {
    *(.bss*)
  }
}