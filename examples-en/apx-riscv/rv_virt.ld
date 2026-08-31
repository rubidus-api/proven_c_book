/* QEMU virt: RAM 은 0x8000_0000 에서 시작한다. */
ENTRY(_start)
SECTIONS {
    . = 0x80000000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    . = ALIGN(16);
    . = . + 4096;          /* 스택 4 KiB */
    _stack_top = .;
}
