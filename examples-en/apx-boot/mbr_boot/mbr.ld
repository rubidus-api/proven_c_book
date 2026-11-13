/* MBR: runs after moving to 0x0600. If code and string constants pass 440 bytes, run.sh stops. */
SECTIONS {
  . = 0x0600;
  .text : { *(.start) *(.text*) *(.rodata*) *(.data*) *(.bss*) }
  /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
