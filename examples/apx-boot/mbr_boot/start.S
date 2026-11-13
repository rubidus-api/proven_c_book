/* BIOS 는 첫 섹터를 0x7c00 에 싣고 DL 에 드라이브 번호를 넣어 여기로 뛴다.
   레지스터와 세그먼트는 아무것도 약속되어 있지 않으므로 먼저 차린다. */
    .code16
    .section .start, "ax"
    .globl _start
_start:
// [excerpt relocate]
    cli                     /* 스택을 바꾸는 동안 인터럽트를 막는다 */
    xor   %ax, %ax
    mov   %ax, %ds          /* 세그먼트를 모두 0 으로: 주소 = 오프셋 */
    mov   %ax, %es
    mov   %ax, %ss
    mov   $0x7c00, %sp      /* 스택은 실린 코드 바로 아래로 자란다 */
    cld
    mov   $0x7c00, %si      /* 자기 자신 512바이트를 0x0600 으로 옮긴다 --- */
    mov   $0x0600, %di      /* 0x7c00 은 다음 단계(파티션의 첫 섹터)가 쓸 자리다 */
    mov   $256, %cx
    rep movsw
    ljmp  $0, $1f           /* 옮긴 쪽으로 뛴다(링크 주소가 0x0600 이다) */
1:  sti
    movzbl %dl, %eax        /* mbr_main(드라이브 번호, 옮긴 파티션 표) */
    mov   $0x07be, %edx
    calll mbr_main
// [/excerpt]
