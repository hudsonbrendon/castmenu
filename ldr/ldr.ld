OUTPUT_FORMAT("elf32-shl")
OUTPUT_ARCH(sh)
ENTRY(_start)
MEMORY { ram : ORIGIN = 0x8CE00000, LENGTH = 0x10000 }
SECTIONS {
  .text : { KEEP(*(.text.start)) *(.text*) *(.rodata*) } > ram
  .data : { *(.data*) } > ram
  . = ALIGN(4);
  __bss_start = .;
  .bss : { *(.bss*) *(COMMON) } > ram
  . = ALIGN(4);
  _end = .;
  /DISCARD/ : { *(.comment) *(.note*) }
}
