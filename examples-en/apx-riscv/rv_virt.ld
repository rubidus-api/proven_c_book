/* QEMU virt: RAM starts at 0x8000_0000. */
ENTRY(_start)
SECTIONS {
    . = 0x80000000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    . = ALIGN(16);
    . = . + 4096;          /* stack 4 KiB */
    _stack_top = .;
}
