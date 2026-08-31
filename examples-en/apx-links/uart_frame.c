/* UART 프레임을 비트로 짓고, *보율이 어긋난 수신기*로 다시 읽어 본다.
   하드웨어는 없다 --- 선 위의 전압을 잘게 썬 배열로 흉내 낸다.
   한 비트를 16칸으로 나누어(흔한 UART 가 정말 이렇게 샘플링한다) 시간을 표현한다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define OS      16u        /* oversampling --- 한 비트를 몇 칸으로 나눌까 */
#define MAXW    4096u

/* 8N1: 시작 1 + 자료 8 + 정지 1 = 10비트. 자료는 *낮은 자리부터* 나간다. */
static size_t make_frame(unsigned char *w, uint8_t byte, unsigned parity_bits)
{
    size_t n = 0;
    for (unsigned i = 0; i < OS * 2; i++) w[n++] = 1;      /* 놀고 있을 때는 높다 */
    for (unsigned i = 0; i < OS; i++)     w[n++] = 0;      /* 시작 비트: 떨어뜨린다 */
    unsigned ones = 0;
    for (int b = 0; b < 8; b++) {
        unsigned bit = byte >> b & 1;                       /* LSB first */
        ones += bit;
        for (unsigned i = 0; i < OS; i++) w[n++] = (unsigned char)bit;
    }
    if (parity_bits) {                                      /* 짝수 패리티라면 */
        unsigned p = ones & 1;                              /* 1의 개수를 짝수로 맞춘다 */
        for (unsigned i = 0; i < OS; i++) w[n++] = (unsigned char)p;
    }
    for (unsigned i = 0; i < OS; i++)     w[n++] = 1;      /* 정지 비트: 다시 높다 */
    for (unsigned i = 0; i < OS * 2; i++) w[n++] = 1;
    return n;
}

/* 수신기: 시작 비트의 내려감을 찾고, 제 비트 길이로 한가운데를 찍어 읽는다.
   rx_bit 이 16 이 아니면 그만큼 보율이 어긋난 것이다. */
static int receive(const unsigned char *w, size_t n, double rx_bit,
                   uint8_t *out, int *stop_ok)
{
    size_t edge = 0;
    while (edge < n && w[edge] != 0) edge++;                /* 내려가는 자리 */
    if (edge >= n) return -1;

    double t0 = (double)edge;
    uint8_t v = 0;
    for (int b = 0; b < 8; b++) {
        double at = t0 + rx_bit * (b + 1) + rx_bit / 2.0;   /* (b+1)번째 비트의 한가운데 */
        size_t idx = (size_t)(at + 0.5);
        if (idx >= n) return -1;
        v |= (uint8_t)(w[idx] << b);
    }
    double sat = t0 + rx_bit * 9 + rx_bit / 2.0;            /* 정지 비트 자리 */
    *stop_ok = (sat < n) && w[(size_t)(sat + 0.5)] == 1;
    *out = v;
    return 0;
}

static void show_wave(const unsigned char *w, size_t n)
{
    printf("  ");
    for (size_t i = 0; i < n; i += OS / 2) putchar(w[i] ? '-' : '_');
    printf("\n  ");
    /* 비트 경계에 이름을 붙인다 */
    const char *lab[] = { "  ", "  ", "St", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "Sp", "  ", "  " };
    for (size_t i = 0, k = 0; i < n; i += OS, k++)
        printf("%-*s", (int)(OS / (OS / 2)), k < sizeof lab / sizeof *lab ? lab[k] : "  ");
    printf("\n");
}

int main(void)
{
    unsigned char w[MAXW];
    const uint8_t byte = 'K';       /* 0x4B = 0100 1011 */

    printf("== the byte to send ==\n");
    printf("  '%c' = 0x%02X = binary %c%c%c%c%c%c%c%c (most significant first)\n\n", byte, byte,
           "01"[byte >> 7 & 1], "01"[byte >> 6 & 1], "01"[byte >> 5 & 1], "01"[byte >> 4 & 1],
           "01"[byte >> 3 & 1], "01"[byte >> 2 & 1], "01"[byte >> 1 & 1], "01"[byte & 1]);

    size_t n = make_frame(w, byte, 0);
    printf("== the shape on the wire (8N1) --- low=_ high=- ==\n");
    show_wave(w, n);
    printf("  * data goes least significant bit first, so it looks reversed to the eye.\n\n");

    printf("== timing ==\n");
    printf("  %-10s %-14s %-14s %s\n", "baud", "one bit", "one frame (10 bits)", "bytes per second");
    const long bauds[] = { 300, 9600, 19200, 115200, 921600 };
    for (unsigned i = 0; i < sizeof bauds / sizeof *bauds; i++) {
        double bit_us = 1e6 / (double)bauds[i];
        printf("  %-10ld %-14.3f %-14.1f %.0f\n", bauds[i], bit_us, bit_us * 10,
               (double)bauds[i] / 10.0);
    }
    printf("  (microseconds. 8N1 spends 10 bits on one byte, so 20%% is overhead)\n\n");

    printf("== when the receiver's baud rate is off ==\n");
    printf("  the receiver samples the middle using its own bit length; the error shifts it.\n\n");
    printf("  %-12s %-10s %-8s %-8s %s\n", "rx baud", "error", "value read", "stop bit", "result");
    struct { const char *name; double factor; } rx[] = {
        { "9600",   1.00 }, { "9700",   9600.0 / 9700 }, { "9900", 9600.0 / 9900 },
        { "10100",  9600.0 / 10100 }, { "10600", 9600.0 / 10600 }, { "19200", 9600.0 / 19200 },
    };
    for (unsigned i = 0; i < sizeof rx / sizeof *rx; i++) {
        uint8_t got; int stop_ok;
        double rx_bit = OS * rx[i].factor;
        if (receive(w, n, rx_bit, &got, &stop_ok) != 0) { printf("  %-12s could not read\n", rx[i].name); continue; }
        double err = (1.0 / rx[i].factor - 1.0) * 100.0;
        printf("  %-12s %+7.1f%%  0x%02X     %-8s %s\n", rx[i].name, err, got,
               stop_ok ? "ok" : "broken",
               got == byte && stop_ok ? "'K' --- correct"
               : got == byte ? "value right, frame lost"
               : "character corrupted");
    }

    /* 임계점을 *찾아본다* --- 통설을 옮겨 적는 대신 프로그램이 재게 한다 */
    double lo = 0, hi = 0;
    for (double e = -20.0; e <= 20.0; e += 0.05) {
        double rx_bit = OS / (1.0 + e / 100.0);
        uint8_t g; int ok;
        int good = receive(w, n, rx_bit, &g, &ok) == 0 && g == byte && ok;
        if (good && lo == 0 && hi == 0) lo = e;
        if (good) hi = e;
    }
    printf("\n  range of error where the character survives here: %+.1f%% to %+.1f%%\n", lo, hi);
    printf("  * the arithmetic agrees. The stop bit is sampled at 9.5 bit times,\n");
    printf("    and that instant must fall within that bit (one bit wide), so\n");
    printf("    0.5 / 9.5 = %.1f%% is the margin on one side.\n", 0.5 / 9.5 * 100.0);
    printf("  * yet practice uses 2 to 3%% as the rule. This %.1f%% is shared by both ends\n",
           0.5 / 9.5 * 100.0);
    printf("    (the sender drifts too), oscillators move with temperature and supply,\n");
    printf("    and a real receiver samples three points and takes a majority.\n");
    printf("    The theoretical limit and the design budget are different numbers.\n");

    printf("\n== what parity catches ==\n");
    size_t np = make_frame(w, byte, 1);
    uint8_t got; int stop_ok;
    receive(w, np, OS, &got, &stop_ok);
    unsigned ones = 0; for (int b = 0; b < 8; b++) ones += byte >> b & 1;
    printf("  '%c' has %u ones -> even parity bit = %u\n", byte, ones, ones & 1);
    printf("  one flipped bit changes the parity, so it is caught.\n");
    printf("  two flipped bits leave the parity unchanged, so it is not --- parity\n");
    printf("  is a device for noticing an error, not for correcting one.\n");
    return 0;
}
