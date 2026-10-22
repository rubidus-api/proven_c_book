/* How slow is storage --- and how many faces does "slow" have?
   It measures with one temporary file and deletes it at the end. No device is touched directly. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

static double ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static uint64_t st = 0xDEADBEEFCAFEBABEull;
static uint64_t rnd(void) { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; }

#define MiB (1024u * 1024u)
#define FILE_MiB 64u
#define BLK 4096u

int main(void)
{
    const char *path = "./_storage_probe.tmp";
    unsigned char *buf = malloc(MiB);
    memset(buf, 0xA5, MiB);

    printf("== what is being measured ==\n");
    printf("  measured with one %u MiB temporary file. This is not the figure of one device\n", FILE_MiB);
    printf("  but of the filesystem, the cache and the device together --- the whole stack.\n\n");

    /* ── ① writing in order ── */
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) { perror("open"); return 1; }
    double t0 = ns();
    for (unsigned i = 0; i < FILE_MiB; i++)
        if (write(fd, buf, MiB) != (ssize_t)MiB) { perror("write"); return 1; }
    double t1 = ns();
    double write_mbs = FILE_MiB / ((t1 - t0) / 1e9);
    printf("== 1. sequential writing ==\n");
    printf("  writing %u MiB (into cache) : %8.1f MiB/s\n", FILE_MiB, write_mbs);

    /* ── ② making it really reach the disk --- fsync ── */
    t0 = ns(); fsync(fd); t1 = ns();
    double fsync_all_ms = (t1 - t0) / 1e6;
    printf("  one fsync after it          : %8.1f ms  <- this much had not reached the disk\n",
           fsync_all_ms);
    close(fd);

    /* small write + fsync, repeated --- what a database does */
    fd = open(path, O_WRONLY);
    double s[9];
    for (int r = 0; r < 9; r++) {
        t0 = ns();
        for (int i = 0; i < 20; i++) {
            if (write(fd, buf, BLK) != (ssize_t)BLK) { perror("write"); return 1; }
            fsync(fd);
        }
        t1 = ns();
        s[r] = (t1 - t0) / 20.0 / 1e6;         /* milliseconds per fsync */
    }
    qsort(s, 9, sizeof *s, cmp_d);
    double fsync_ms = s[4];
    close(fd);
    printf("  4 KiB write + fsync         : %8.3f ms each -> about %.0f per second\n",
           fsync_ms, 1000.0 / fsync_ms);
    printf("  * that is the cost of one database commit. Which is why designs gather several\n");
    printf("    commits and flush them together (group commit).\n");

    /* ── ③ with and without the cache ── */
    printf("\n== 2. reading --- with the cache, and after asking to drop it ==\n");
    fd = open(path, O_RDONLY);
    double seq_cached = 0, seq_cold = 0, rnd_cached = 0, rnd_cold = 0;

    for (int cold = 0; cold < 2; cold++) {
        if (cold) posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);   /* drop only this file's cache */
        lseek(fd, 0, SEEK_SET);
        t0 = ns();
        for (unsigned i = 0; i < FILE_MiB; i++)
            if (read(fd, buf, MiB) != (ssize_t)MiB) { perror("read"); return 1; }
        t1 = ns();
        double mbs = FILE_MiB / ((t1 - t0) / 1e9);
        if (cold) seq_cold = mbs; else seq_cached = mbs;
    }
    printf("  sequential read (cached)  : %8.1f MiB/s\n", seq_cached);
    printf("  sequential read (dropped) : %8.1f MiB/s  -> a factor of %.1f\n",
           seq_cold, seq_cached / seq_cold);

    /* random 4 KiB reads */
    for (int cold = 0; cold < 2; cold++) {
        if (cold) posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        const int NREAD = 2000;
        t0 = ns();
        for (int i = 0; i < NREAD; i++) {
            off_t off = (off_t)((rnd() % (FILE_MiB * MiB / BLK)) * BLK);
            if (pread(fd, buf, BLK, off) != (ssize_t)BLK) { perror("pread"); return 1; }
        }
        t1 = ns();
        double us = (t1 - t0) / NREAD / 1000.0;
        if (cold) rnd_cold = us; else rnd_cached = us;
    }
    printf("  random 4 KiB (cached)     : %8.1f us each -> %.0f per second\n",
           rnd_cached, 1e6 / rnd_cached);
    printf("  random 4 KiB (dropped)    : %8.1f us each -> %.0f per second\n",
           rnd_cold, 1e6 / rnd_cold);

    /* ★ Check that the request took effect. If the two values are close, the cache stayed. */
    double ratio = seq_cached / seq_cold;
    if (ratio < 1.2 && ratio > 0.8) {
        printf("\n  * the two are nearly equal --- the request to drop the cache was not honoured.\n");
        printf("    This filesystem keeps its own cache, which `posix_fadvise` does not\n");
        printf("    empty. So read the numbers above not as the speed of the device but as\n");
        printf("    the speed of reading a file that is already in memory.\n");
        printf("    (%.0f MiB/s is the speed of memory, not of storage.)\n", seq_cold);
        printf("    Really measuring the device needs bypassing the cache (O_DIRECT) or the\n");
        printf("    privilege to drop the system cache, and this environment has neither ---\n");
        printf("    so what could not be measured is left unmeasured.\n");
    }
    close(fd);
    unlink(path);

    /* ── ④ putting it on the ladder ── */
    printf("\n== 3. placed on the ladder from earlier ==\n");
    printf("  %-28s %14s %s\n", "what", "each", "against L1 (1.4 ns)");
    struct { const char *name; double v_ns; } rows[] = {
        { "L1 cache read",           1.4 },
        { "main memory read",            75.0 },
        { "random 4 KiB (from cache)", rnd_cached * 1000.0 },
        { "4 KiB write + fsync",     fsync_ms * 1e6 },
    };
    for (unsigned i = 0; i < sizeof rows / sizeof *rows; i++) {
        char v[32];
        if (rows[i].v_ns < 1e3)      snprintf(v, sizeof v, "%.1f ns", rows[i].v_ns);
        else if (rows[i].v_ns < 1e6) snprintf(v, sizeof v, "%.1f us", rows[i].v_ns / 1e3);
        else                         snprintf(v, sizeof v, "%.2f ms", rows[i].v_ns / 1e6);
        printf("  %-28s %14s %12.0f x\n", rows[i].name, v, rows[i].v_ns / 1.4);
    }
    printf("\n  * the ladder does not end at the cache. Several more orders of magnitude lie below.\n");
    printf("    And on those lower rungs, reading in order matters far more.\n");

    free(buf);
    return 0;
}
