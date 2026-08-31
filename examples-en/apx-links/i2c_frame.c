/* I2C 거래 하나를 신호 차례로 짓고, 그 차례를 다시 읽어 해독한다.
   선은 둘뿐이다: SCL(클록)과 SDA(자료). 둘 다 「끌어내리기만」 할 수 있다(오픈 드레인). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

enum { EV_START, EV_BIT, EV_ACK, EV_NACK, EV_RSTART, EV_STOP };

struct ev { int kind; int val; const char *note; };

static struct ev log_[256];
static int n_ev;

static void put(int kind, int val, const char *note)
{ log_[n_ev++] = (struct ev){ kind, val, note }; }

/* 바이트 하나 --- 높은 자리부터 여덟 비트, 그다음 아홉 번째 클록이 ACK 자리 */
static void put_byte(uint8_t b, const char *what, int acked)
{
    for (int i = 7; i >= 0; i--) put(EV_BIT, b >> i & 1, i == 7 ? what : NULL);
    put(acked ? EV_ACK : EV_NACK, acked ? 0 : 1, NULL);
}

static const char *kind_name(int k)
{
    switch (k) {
    case EV_START:  return "START";
    case EV_RSTART: return "repeated START";
    case EV_STOP:   return "STOP";
    case EV_ACK:    return "ACK";
    case EV_NACK:   return "NACK";
    default:        return "bits";
    }
}

int main(void)
{
    const uint8_t dev = 0x3C;      /* 7비트 장치 주소 */
    const uint8_t reg = 0x00, val = 0xAF;

    printf("== why the address is confusing ==\n");
    printf("  the 7-bit address 0x%02X is shifted one place on the wire (last bit is read/write).\n", dev);
    printf("    write: 0x%02X << 1 | 0 = 0x%02X\n", dev, dev << 1);
    printf("    read : 0x%02X << 1 | 1 = 0x%02X\n", dev, dev << 1 | 1);
    printf("  so datasheets call the same device 0x%02X in one place and 0x%02X in another.\n\n",
           dev, dev << 1);

    /* ── 쓰기 거래: 장치에게 「레지스터 0 에 0xAF 를 써라」 ── */
    put(EV_START, 0, "the controller takes the bus");
    put_byte((uint8_t)(dev << 1 | 0), "device address + write", 1);
    put_byte(reg, "register number", 1);
    put_byte(val, "value to write", 1);
    put(EV_STOP, 0, "release the bus");

    /* ── 읽기 거래: 반복 START 로 방향만 바꾼다 ── */
    put(EV_START, 0, "take it again");
    put_byte((uint8_t)(dev << 1 | 0), "device address + write", 1);
    put_byte(reg, "say which register to read first", 1);
    put(EV_RSTART, 0, "* START again without STOP --- nobody can cut in meanwhile");
    put_byte((uint8_t)(dev << 1 | 1), "device address + read", 1);
    put_byte(val, "the value the device returned", 0);   /* 마지막 바이트는 주인이 NACK 로 끝을 알린다 */
    put(EV_STOP, 0, "end");

    printf("== the order on the wire ==\n");
    int bitpos = 0; uint8_t acc = 0;
    for (int i = 0; i < n_ev; i++) {
        struct ev *e = &log_[i];
        if (e->kind == EV_BIT) {
            acc = (uint8_t)(acc << 1 | e->val);
            if (++bitpos == 8) {
                printf("  %-12s 0x%02X  %s\n", "byte", acc,
                       log_[i - 7].note ? log_[i - 7].note : "");
                bitpos = 0; acc = 0;
            }
        } else {
            printf("  %-12s %s%s\n", kind_name(e->kind),
                   e->kind == EV_ACK ? "the device pulled SDA down = received" :
                   e->kind == EV_NACK ? "nobody pulled it down = absent, or done" : "",
                   e->note ? e->note : "");
        }
    }

    printf("\n== what means what ==\n");
    printf("  START : SDA falls while SCL is high   (deliberately breaking the rule that\n");
    printf("  STOP  : SDA rises while SCL is high    data changes only while SCL is low)\n");
    printf("  ACK   : on the ninth clock the receiver pulls SDA down\n");
    printf("  NACK  : nobody pulls it down and the line stays high --- absent and enough look alike\n");

    printf("\n== timing ==\n");
    int bits = 0, extra = 0;
    for (int i = 0; i < n_ev; i++)
        (log_[i].kind == EV_BIT) ? bits++ : (log_[i].kind == EV_ACK || log_[i].kind == EV_NACK) ? bits++ : extra++;
    printf("  clocks in these two transactions: %d (data and ACK bits), %d markers\n", bits, extra);
    printf("  %-14s %-14s %s\n", "speed mode", "clock", "time for these two transactions");
    struct { const char *name; double hz; } modes[] = {
        { "standard", 100e3 }, { "fast", 400e3 }, { "fast plus", 1e6 }, { "high speed", 3.4e6 },
    };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++)
        printf("  %-14s %-14.1f %.1f microseconds\n", modes[i].name, modes[i].hz / 1000,
               bits / modes[i].hz * 1e6);
    printf("  (clock in kHz; markers and wait states are not counted)\n");

    printf("\n== why the lines are only ever pulled down ==\n");
    printf("  if two devices speak at once, one pushing 5V and one pushing 0V, current\n");
    printf("  flows straight through and damages the chips. I2C lets nobody push (open drain)\n");
    printf("  and leaves the raising to a resistor. So speaking at once is safe,\n");
    printf("  and the property that zero wins gives arbitration and ACK for free.\n");
    return 0;
}
