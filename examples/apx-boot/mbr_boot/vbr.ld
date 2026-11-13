/* VBR: 0x7c00 에 실린다. 첫 섹터에는 입구와 나머지를 싣는 코드만 두고 510 번에 55 aa 를,
   그 뒤(0x7e00 부터)에 본체를 둔다. */
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
