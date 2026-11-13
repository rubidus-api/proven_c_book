/* 진짜 백엔드: BIOS 의 int 13h 와 직렬 포트를 부른다. 16비트 실모드 코드로 짓는다(gcc -m16).
   QEMU 의 PC 기계와 SeaBIOS 에서 돌렸고, 진짜 PC 의 펌웨어에서는 돌려 보지 않았다. */
#include "bios.h"

static inline void outb(uint16_t port, uint8_t v)
{ __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port)); }
static inline uint8_t inb(uint16_t port)
{ uint8_t v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }

// [excerpt int13]
/* COM1: 보낼 칸이 비면(LSR 비트 5) 한 바이트 */
void con_putc(char c)
{
    while (!(inb(0x3fd) & 0x20)) { }
    outb(0x3f8, (uint8_t)c);
}

int disk_read(uint8_t drive, uint32_t lba, void *dst)
{
    /* int 13h 확장 읽기(AH=42h)에 넘기는 16바이트 */
    struct [[gnu::packed]] {
        uint8_t  size, zero;
        uint16_t count, offset, segment;
        uint32_t lba_low, lba_high;
    } dap = { 16, 0, 1, (uint16_t)(uintptr_t)dst, 0, lba, 0 };
    uint16_t ax = 0x4200;
    uint8_t failed;
    /* 실패하면 BIOS 가 CF 를 세운다 */
    __asm__ volatile("int $0x13; setc %1"
                     : "+a"(ax), "=qm"(failed)
                     : "d"(drive), "S"(&dap)
                     : "memory", "cc");
    return failed;
}

void chain(uint8_t drive, const struct part *entry, void *code)
{
    __asm__ volatile("jmp *%2" : : "d"(drive), "S"(entry), "r"(code));
    __builtin_unreachable();
}
// [/excerpt]

uint8_t *load_area(void) { return (uint8_t *)0x7c00; }
uint16_t real_addr(const void *p) { return (uint16_t)(uintptr_t)p; }

/* QEMU 의 isa-debug-exit 장치(포트 0xf4)에 쓰면 에뮬레이터가 끝난다 --- 시연을 끝내는 자리일 뿐,
   진짜 PC 라면 여기서 int 18h(「다음 장치로」)를 부르거나 멈춰 선다. */
void stop(uint8_t code)
{
    outb(0xf4, code);
    for (;;) __asm__ volatile("hlt");
}
