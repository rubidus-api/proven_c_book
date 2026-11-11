/* Read the flash on SPI controller 0 (0x1004_0000) of QEMU's sifive_u machine. The
   address is the value in the spi@10040000 and flash@0 entries of the device tree QEMU
   provides. */
#include "spi.h"
#include "hw.h"

/* sifive_u has no device that shuts QEMU down. End with semihosting (an agreed
   instruction triple that tells a debugger "done"). The build for an ordinary PC does
   nothing here. */
static void finish(void)
{
#ifdef __riscv
    /* "application exited", exit code 0 */
    static const long block[2] = { 0x20026, 0 };
    register long a0 __asm__("a0") = 0x18;            /* SYS_EXIT */
    /* on 64-bit, pass the address of a two-word block */
    register const long *a1 __asm__("a1") = block;
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
