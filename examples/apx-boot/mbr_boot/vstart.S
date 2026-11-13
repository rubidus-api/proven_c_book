/* VBR(파티션의 첫 섹터)의 입구. MBR 이 0x7c00 에 싣고 DL = 드라이브, SI = 표의 칸으로 넘긴다. */
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
    movzbl %dl, %eax        /* vbr_boot(드라이브 번호, MBR 이 넘긴 칸) */
    movzwl %si, %edx
    calll vbr_boot
