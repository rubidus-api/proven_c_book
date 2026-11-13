/* MBR: 0x0600 으로 옮겨 가 돈다. 코드와 상수 문자열이 440바이트를 넘으면 run.sh 가 멈춘다. */
SECTIONS {
  . = 0x0600;
  .text : { *(.start) *(.text*) *(.rodata*) *(.data*) *(.bss*) }
  /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
