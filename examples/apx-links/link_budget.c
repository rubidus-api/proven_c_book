/* 같은 일을 폴링·인터럽트·DMA 로 할 때 무엇이 얼마나 드는가 --- 예산 계산.
   하드웨어가 없으므로 이것은 *계산*이다. 값(비용)은 흔히 인용되는 자릿수를 가정으로
   두고, 그 가정을 화면에 함께 적는다 --- 가정을 감추면 계산이 아니라 주장이 된다. */
#include <stdio.h>

/* 가정 --- 요즘 흔한 마이크로컨트롤러/PC 급의 어림값 */
#define IRQ_COST_US   1.0     /* 인터럽트 한 번 처리에 드는 시간 */
#define POLL_COST_US  0.2     /* 폴링 한 번(레지스터 읽고 판정)에 드는 시간 */

struct link { const char *name; double bytes_per_sec; };

static void budget(struct link L, double dma_block)
{
    double irq_rate  = L.bytes_per_sec;                  /* 바이트마다 끼어들기 */
    double irq_busy  = irq_rate * IRQ_COST_US / 1e6 * 100.0;
    double dma_rate  = L.bytes_per_sec / dma_block;      /* 덩어리마다 한 번 */
    double dma_busy  = dma_rate * IRQ_COST_US / 1e6 * 100.0;

    char a[24], b[24];
    snprintf(a, sizeof a, "%.1f%%", irq_busy);
    snprintf(b, sizeof b, "%.3f%%", dma_busy);
    printf("  %-22s %12.0f %10s %12.0f %10s\n", L.name, irq_rate, a, dma_rate, b);
}

int main(void)
{
    printf("== assumptions (the values used) ==\n");
    printf("  handling one interrupt    : %.1f microseconds\n", IRQ_COST_US);
    printf("  one poll (read and judge) : %.1f microseconds\n", POLL_COST_US);
    printf("  DMA batch size            : 256 bytes\n");
    printf("  * all three differ by machine. Change them and the conclusion changes --- that is the point.\n\n");

    printf("== an interrupt per byte vs DMA in 256-byte batches ==\n");
    printf("  %-22s %12s %10s %12s %10s\n", "link", "IRQ/s", "CPU", "DMA IRQ/s", "CPU");
    struct link links[] = {
        { "UART 9600 8N1",       960 },
        { "UART 115200 8N1",   11520 },
        { "I2C 400 kHz",       44000 },
        { "SPI 10 MHz",      1250000 },
        { "USB 2.0 (effective)",  30000000 },
        { "1 Gbit Ethernet",   118000000 },
    };
    for (unsigned i = 0; i < sizeof links / sizeof *links; i++) budget(links[i], 256);

    printf("\n  * reading the table downwards shows when DMA becomes necessary.\n");
    printf("    on a slow link an interrupt per byte is no trouble; past some point\n");
    printf("    that scheme leaves the machine doing nothing else.\n");

    printf("\n== where the crossover is (assuming an interrupt per byte) ==\n");
    printf("  %-14s %-16s %s\n", "CPU used", "events/s", "the data rate that means");
    for (double busy = 5; busy <= 100; busy *= 2) {
        double rate = busy / 100.0 * 1e6 / IRQ_COST_US;   /* 사건/초 */
        char pct[16]; snprintf(pct, sizeof pct, "%.0f%%", busy);
        printf("  %-14s %-16.0f %.1f KiB/s\n", pct, rate, rate / 1024.0);
    }

    /* 한글은 한 글자가 두 칸이라 %-Ns 로는 안 맞는다 --- 줄마다 손으로 적는다 */
    printf("\n== the three schemes compared ==\n");
    printf("  polling   : the CPU keeps asking.      latency = about half the period.\n");
    printf("              suits very frequent events, or very simple places.\n");
    printf("  interrupt : it wakes only when there is work. latency = the cost of waking.\n");
    printf("              suits events that come now and then.\n");
    printf("  DMA       : it wakes only when a batch ends. latency = until the batch fills.\n");
    printf("              suits data that is large and steady.\n");

    printf("\n== there are places where polling is better ==\n");
    double poll_hz[] = { 1e3, 1e4, 1e5, 1e6 };
    printf("  %-16s %-14s %s\n", "poll rate", "CPU used", "mean latency");
    for (unsigned i = 0; i < 4; i++) {
        char pct[16]; snprintf(pct, sizeof pct, "%.2f%%", poll_hz[i] * POLL_COST_US / 1e6 * 100.0);
        printf("  %-11.0f /s %-14s %.1f microseconds\n", poll_hz[i], pct, 1e6 / poll_hz[i] / 2);
    }
    printf("  * when events are very frequent polling wins --- there is always work, so\n");
    printf("    there is no reason to pay the cost of waking each time. That is why fast\n");
    printf("    network drivers switch to polling under load (Linux NAPI).\n");
    return 0;
}
