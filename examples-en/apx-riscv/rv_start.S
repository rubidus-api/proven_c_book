/* Entry point of the QEMU virt machine --- set up the stack and go to main.
   Booted with -bios none, execution starts at 0x8000_0000. */
    .section .text.start, "ax"
    .globl _start
_start:
    la   sp, _stack_top
    call main
1:  wfi
    j    1b
