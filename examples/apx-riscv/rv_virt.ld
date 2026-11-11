/* QEMU virt: RAM 은 0x8000_0000 에서 시작한다. */
ENTRY(_start)
SECTIONS {
    . = 0x80000000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) *(.srodata*) }
    .data : { *(.data*) *(.sdata*) }
    . = ALIGN(8);
    _bss_start = .;
    .bss : { *(.sbss*) *(.bss*) *(COMMON) }
    . = ALIGN(8);
    _bss_end = .;
    . = ALIGN(16);
    . = . + 16384;         /* 스택 16 KiB */
    _stack_top = .;
}
