/* The VBR's entry (the partition's first sector). The MBR loads it at 0x7c00 and hands over
   with DL = drive, SI = the table entry. */
    .code16
    .section .start, "ax"
    .globl _start
_start:
    cli
    xor   %ax, %ax
    mov   %ax, %ds
    mov   %ax, %es
    mov   %ax, %ss
    mov   $0x7c00, %sp
    sti
    movzbl %dl, %eax        /* vbr_boot(drive number, the entry the MBR passed) */
    movzwl %si, %edx
    calll vbr_boot
