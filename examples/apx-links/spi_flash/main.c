/* QEMU sifive_u 기계의 0번 SPI 제어기(0x1004_0000)에 달린 플래시를 읽는다.
   주소는 QEMU 가 내주는 장치 트리의 spi@10040000 · flash@0 항목에 적힌 값이다. */
#include "spi.h"
#include "hw.h"

/* sifive_u 에는 QEMU 를 끄는 장치가 없다. 반호스팅(디버거에게 「끝났다」고 알리는
   약속된 명령 셋)으로 끝낸다. 보통 PC 에서 짓는 판에서는 아무것도 하지 않는다. */
static void finish(void)
{
#ifdef __riscv
    static const long block[2] = { 0x20026, 0 };      /* 「응용이 끝났다」, 종료 코드 0 */
    register long a0 __asm__("a0") = 0x18;            /* SYS_EXIT */
    register const long *a1 __asm__("a1") = block;    /* 64비트에서는 두 낱말의 자리를 넘긴다 */
    __asm__ volatile (".option push\n.option norvc\n"
                      "slli x0, x0, 0x1f\nebreak\nsrai x0, x0, 7\n.option pop"
                      :: "r"(a0), "r"(a1) : "memory");
#endif
}

int main(void)
{
    const struct spi spi0 = { .base = 0x10040000u };
    spi_init(&spi0, 3);

    uint8_t id[3];
    flash_jedec_id(&spi0, id);
    uint8_t text[9] = { 0 };
    flash_read(&spi0, 0, text, 8);

    hw_log("JEDEC id "); for (int i = 0; i < 3; i++) { hw_log_hex(id[i], 2); hw_log(" "); }
    hw_log(id[0] == 0x9D ? "(manufacturer 0x9d = ISSI)\n" : "\n");
    hw_log("first 8 bytes of the flash: ");
    hw_log((const char *)text);
    finish();
    return 0;
}
