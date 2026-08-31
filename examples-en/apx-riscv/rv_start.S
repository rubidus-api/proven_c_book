/* QEMU virt 기계의 시작점 --- 스택을 세우고 main 으로 간다.
   -bios none 으로 올리면 0x8000_0000 에서 실행이 시작된다. */
    .section .text.start, "ax"
    .globl _start
_start:
    la   sp, _stack_top
    call main
1:  wfi
    j    1b
