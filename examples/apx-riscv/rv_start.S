/* QEMU virt · sifive_u 기계의 시작점 --- 0번 하트만 남기고, .bss 를 0 으로 채우고,
   스택을 세우고, main 으로 간다. -bios none 으로 올리면 0x8000_0000 에서 시작한다.
   sifive_u 는 하트 여럿이 함께 깨어나므로 나머지는 잠재운다. */
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
