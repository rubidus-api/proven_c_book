/* Entry point of the QEMU virt and sifive_u machines --- keep hart 0 only, zero .bss,
   set up the stack and go to main. Booted with -bios none, execution starts at
   0x8000_0000. sifive_u wakes several harts together, so the others are put to sleep.
*/
    .section .text.start, "ax"
    .globl _start
_start:
    csrr t0, mhartid
    bnez t0, 3f
    la   t0, _bss_start
    la   t1, _bss_end
1:  bgeu t0, t1, 2f
    sd   zero, 0(t0)
    addi t0, t0, 8
    j    1b
2:  la   sp, _stack_top
    call main
3:  wfi
    j    3b
