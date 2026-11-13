/* VBR: loaded at 0x7c00. The first sector holds only the entry and the code that loads the
   rest, with 55 aa at 510; the body follows from 0x7e00. */
SECTIONS {
  . = 0x7c00;
  .boot : { vstart.o(.start) vboot.o(.text*) bios_int.o(.text*) }
  . = 0x7dfe;
  .sig : { SHORT(0xaa55) }
  .text : { *(.text*) *(.rodata*) *(.data*) }
  _load_end = .;
  .bss : { *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
