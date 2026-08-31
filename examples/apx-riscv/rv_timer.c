/* RISC-V 베어메탈: 타이머 인터럽트를 걸고, 처리기는 깃발만 세우고 나온다.
   QEMU 의 virt 기계에서 실제로 돌려 출력을 캡처했다
   (UART 0x1000_0000, CLINT 0x0200_0000, 종료 장치 0x0010_0000). */
#include <stdint.h>

#define UART0   ((volatile uint8_t  *)0x10000000u)
#define MTIME   ((volatile uint64_t *)0x0200bff8u)
#define MTIMECMP ((volatile uint64_t *)0x02004000u)
#define FINISH  ((volatile uint32_t *)0x00100000u)  /* QEMU 를 끄는 자리 */
#define TICK    10000000u                    /* 10 MHz 기준 1초 */

static volatile uint32_t ticks = 0;          /* 처리기와 주 루프가 함께 보는 값 */

static void uart_puts(const char *s) { while (*s) *UART0 = (uint8_t)*s++; }

/* 하드웨어가 레지스터를 하나도 저장해 주지 않으므로 컴파일러에게 시킨다.
   이 속성이 하는 일: 쓰는 레지스터를 전부 저장·복원하고 ret 대신 mret 로 끝낸다.

   ★ aligned(4) 를 빠뜨리면 조용히 무너진다. mtvec 의 하위 두 비트는 주소가 아니라
   MODE 필드다. 압축 명령(C 확장)이 있으면 함수가 2바이트 경계에 놓일 수 있고,
   그 주소를 mtvec 에 쓰면 MODE 가 2(예약)가 되어 트랩이 엉뚱한 곳으로 간다. */
__attribute__((interrupt("machine"), aligned(4)))
void trap_handler(void)
{
    *MTIMECMP = *MTIME + TICK;               /* 다음 알람을 예약한다 */
    ticks++;                                 /* 깃발만 세우고 곧바로 나온다 */
}

int main(void)
{
    extern void trap_handler(void);
    uintptr_t vec = (uintptr_t)&trap_handler;
    __asm__ volatile ("csrw mtvec, %0" :: "r"(vec));       /* 트랩은 여기로 */
    *MTIMECMP = *MTIME + TICK;
    __asm__ volatile ("csrs mie, %0"    :: "r"(1u << 7));  /* 타이머 인터럽트 허용 */
    __asm__ volatile ("csrs mstatus, %0" :: "r"(1u << 3)); /* 전역 허용(MIE) */

    uart_puts("waiting for the timer...\n");
    uint32_t seen = 0;
    for (;;) {
        if (ticks != seen) {                 /* 일은 주 루프가 한다 */
            seen = ticks;
            uart_puts("tick\n");
            if (seen == 3) {
                uart_puts("three ticks; done\n");
                *FINISH = 0x5555u;           /* 에뮬레이터를 끈다 */
            }
        }
        __asm__ volatile ("wfi");            /* 할 일이 없으면 잔다 */
    }
    for (;;) { }                             /* 돌아갈 곳이 없다 */
}
