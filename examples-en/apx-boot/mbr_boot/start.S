/* The BIOS loads the first sector at 0x7c00, puts the drive number in DL and jumps here.
   Nothing is promised about the registers and segments, so they are set up first. */
    .code16
    .section .start, "ax"
    .globl _start
_start:
// [excerpt relocate]
    cli                     /* no interrupts while the stack changes */
    xor   %ax, %ax
    mov   %ax, %ds          /* all segments 0: address = offset */
    mov   %ax, %es
    mov   %ax, %ss
    mov   $0x7c00, %sp      /* the stack grows down from just below the code */
    cld
    mov   $0x7c00, %si      /* move our own 512 bytes to 0x0600 --- */
    mov   $0x0600, %di      /* 0x7c00 is for the next stage, the partition's sector 0 */
    mov   $256, %cx
    rep movsw
    ljmp  $0, $1f           /* jump into the moved copy (linked at 0x0600) */
1:  sti
    movzbl %dl, %eax        /* mbr_main(drive number, the moved partition table) */
    mov   $0x07be, %edx
    calll mbr_main
// [/excerpt]
