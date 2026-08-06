/* Two-byte Johab Hangul — a real case of one word split into initial,
   medial and final jamo. (Hangul appears here because the subject is an
   encoding of Hangul.) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Names of the jamo. Codes 0 and 1 are fill/unused, so they stay empty. */
static const char *const CHO[32] = {
    [2]="G", [3]="GG", [4]="N", [5]="D", [6]="DD", [7]="R", [8]="M",
    [9]="B", [10]="BB", [11]="S", [12]="SS", [13]="NG", [14]="J",
    [15]="JJ", [16]="C", [17]="K", [18]="T", [19]="P", [20]="H",
};
static const char *const JUNG[32] = {
    [3]="A", [4]="AE", [5]="YA", [6]="YAE", [7]="EO",
    [10]="E", [11]="YEO", [12]="YE",
    [13]="O", [14]="WA", [15]="WAE",
    [18]="OE", [19]="YO", [20]="U", [21]="WEO", [22]="WE", [23]="WI",
    [26]="YU", [27]="EU", [28]="YI", [29]="I",
};
static const char *const JONG[32] = {
    [1]="(none)", [2]="G", [5]="N", [9]="L", [17]="M", [19]="B",
    [21]="S", [23]="NG", [29]="H",
};

/* (1) Extract with shifts and masks — the way the standard pins down */
static void split_by_shift(uint16_t w)
{
    unsigned cho  = (w >> 10) & 0x1f;
    unsigned jung = (w >>  5) & 0x1f;
    unsigned jong =  w        & 0x1f;
    printf("  shift/mask: flag=%u cho=%2u(%-2s) jung=%2u(%-3s) jong=%2u(%s)\n",
           (unsigned)(w >> 15), cho, CHO[cho] ? CHO[cho] : "?",
           jung, JUNG[jung] ? JUNG[jung] : "?",
           jong, JONG[jong] ? JONG[jong] : "?");
}

/* (2) View it with bit fields in a union — handy, but the layout is
   implementation-defined */
union johab {
    uint16_t raw;
    struct {
        uint16_t jong : 5;   /* this order is not promised by the standard */
        uint16_t jung : 5;
        uint16_t cho  : 5;
        uint16_t mark : 1;
    } f;
};

int main(void)
{
    /* The Johab codes of 가 and 한, taken from the real table. */
    const uint16_t GA  = 0x8861;   /* 1 00010 00011 00001 */
    const uint16_t HAN = 0xD065;   /* 1 10100 00011 00101 */

    printf("GA(가) = 0x%04X, HAN(한) = 0x%04X\n\n", GA, HAN);
    puts("GA (가):");  split_by_shift(GA);
    puts("HAN (한):"); split_by_shift(HAN);

    union johab u = { .raw = GA };
    printf("\nthrough bit fields: mark=%u cho=%u jung=%u jong=%u\n",
           u.f.mark, u.f.cho, u.f.jung, u.f.jong);
    puts("  (On this compiler it matched the shift/mask result, because this");
    puts("   implementation fills bits from the low end. That is not a promise.)");

    /* (3) The second byte collides with ASCII — Johab's famous trap */
    unsigned char bytes[3] = { (unsigned char)(GA >> 8),
                               (unsigned char)(GA & 0xff), 0 };
    printf("\nthe two bytes of 가: %02X %02X\n", bytes[0], bytes[1]);
    printf("  the second byte 0x%02X is ASCII '%c' — searching bytes for 'a'\n",
           bytes[1], bytes[1]);
    printf("  lands inside a character: strchr result = %s\n",
           strchr((char *)bytes, 'a') ? "found (false hit)" : "not found");
    return 0;
}
