/* QEMU virt: RAM starts at 0x8000_0000. */
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
    . = . + 16384;         /* 16 KiB stack */
    _stack_top = .;
}
