/* SPI 는 「고리 모양으로 이어 붙인 시프트 레지스터 둘」이다.
   클록마다 한 비트씩 서로 밀어 넣는다 --- 그래서 주고받기가 *동시에* 일어난다. */
#include <stdio.h>
#include <stdint.h>

static void bits8(uint8_t v, char *out)
{ for (int i = 0; i < 8; i++) out[i] = (char)('0' + (v >> (7 - i) & 1)); out[8] = 0; }

int main(void)
{
    uint8_t m = 0xA5;      /* 주인이 보낼 값 */
    uint8_t s = 0x3C;      /* 장치가 보낼 값 (미리 제 레지스터에 넣어 둔다) */
    char mb[9], sb[9];

    bits8(m, mb); bits8(s, sb);
    printf("== at the start ==\n");
    printf("  controller register : 0x%02X (%s)\n", m, mb);
    printf("  device register     : 0x%02X (%s)\n\n", s, sb);

    printf("== one bit per clock ==\n");
    printf("  %-6s %-6s %-6s %-12s %-12s\n", "clock", "MOSI", "MISO", "controller", "device");
    for (int c = 1; c <= 8; c++) {
        unsigned mosi = m >> 7 & 1;          /* 주인이 내놓는 비트 (높은 자리부터) */
        unsigned miso = s >> 7 & 1;          /* 장치가 내놓는 비트 */
        m = (uint8_t)(m << 1 | miso);        /* 서로의 비트를 낮은 자리로 받아 넣는다 */
        s = (uint8_t)(s << 1 | mosi);
        bits8(m, mb); bits8(s, sb);
        printf("  %-6d %-6u %-6u %-12s %-12s\n", c, mosi, miso, mb, sb);
    }

    printf("\n== after eight clocks ==\n");
    printf("  controller register : 0x%02X  <- what the device sent\n", m);
    printf("  device register     : 0x%02X  <- what the controller sent\n", s);
    printf("  * sending and receiving finished in the same eight clocks. Full duplex is\n");
    printf("    free because the two registers form one ring. Even with nothing to receive\n");
    printf("    something must be sent (usually 0x00 or 0xFF): a dummy byte.\n");

    printf("\n== the four modes --- which edge presents, which samples ==\n");
    printf("  %-8s %-6s %-6s %-16s %s\n", "mode", "CPOL", "CPHA", "SCK at rest", "sampling edge");
    for (int mode = 0; mode < 4; mode++) {
        int cpol = mode >> 1, cpha = mode & 1;
        printf("  %-8d %-6d %-6d %-16s %s\n", mode, cpol, cpha,
               cpol ? "high" : "low",
               cpha == 0 ? (cpol ? "falling (first edge)" : "rising (first edge)")
                         : (cpol ? "rising (second edge)" : "falling (second edge)"));
    }

    printf("\n== when the mode is mismatched ==\n");
    uint8_t src = 0xA5, wrong = 0, prev = 0;
    for (int c = 0; c < 8; c++) {
        unsigned bit = src >> (7 - c) & 1;
        wrong = (uint8_t)(wrong << 1 | prev);   /* 한 모서리 늦게 읽으면 *직전* 비트를 본다 */
        prev = bit;
    }
    bits8(0xA5, mb); bits8(wrong, sb);
    printf("  sent 0xA5 (%s)\n", mb);
    printf("  read with one edge of error: 0x%02X (%s)  <- everything shifted one place\n", wrong, sb);
    printf("  * so when SPI gives half-right values, suspect the mode first.\n");

    printf("\n== speed ==\n");
    printf("  SPI has no start, stop, address or ACK. Exactly eight clocks per byte.\n");
    printf("  %-14s %-16s %s\n", "SCK", "bytes per second", "compared");
    struct { const char *name; double hz; } sck[] = {
        { "1 MHz", 1e6 }, { "10 MHz", 10e6 }, { "50 MHz", 50e6 },
    };
    const double i2c_std = 100e3 / 9.0;      /* I2C 표준 모드: 바이트마다 9비트 */
    for (unsigned i = 0; i < sizeof sck / sizeof *sck; i++)
        printf("  %-14s %-16.0f %.0f times I2C standard (100 kHz)\n", sck[i].name, sck[i].hz / 8,
               (sck[i].hz / 8) / i2c_std);
    printf("  (UART spends 10 bits per byte, I2C 9, SPI 8 --- no overhead)\n");
    return 0;
}
