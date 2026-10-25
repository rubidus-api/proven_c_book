/* Writing field by field versus gathering into one buffer first.
   `sink` stands in for whatever is expensive per call --- send() on a socket,
   write() on a file, a USB transfer. It only counts. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned long calls, bytes;
static void sink(const void *p, size_t n) { (void)p; calls++; bytes += n; }

struct point { int32_t x, y; uint16_t id; };

static void put_le(unsigned char *b, uint32_t v, int n)
{ for (int i = 0; i < n; i++) b[i] = (unsigned char)(v >> (8 * i)); }

/* (1) every field goes out on its own */
static void send_each(const struct point *pts, size_t count)
{
    unsigned char b[4];
    for (size_t i = 0; i < count; i++) {
        put_le(b, (uint32_t)pts[i].x, 4);  sink(b, 4);
        put_le(b, (uint32_t)pts[i].y, 4);  sink(b, 4);
        put_le(b, pts[i].id, 2);           sink(b, 2);
    }
}

/* (2) fill a buffer, send it when it is full, and once more at the end */
static void send_batched(const struct point *pts, size_t count)
{
    unsigned char buf[4096];
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        if (sizeof buf - used < 10) { sink(buf, used); used = 0; }
        put_le(buf + used, (uint32_t)pts[i].x, 4);
        put_le(buf + used + 4, (uint32_t)pts[i].y, 4);
        put_le(buf + used + 8, pts[i].id, 2);
        used += 10;
    }
    if (used) sink(buf, used);
}

int main(void)
{
    static struct point pts[10000];
    for (size_t i = 0; i < 10000; i++)
        pts[i] = (struct point){ (int32_t)i, -(int32_t)i, (uint16_t)i };

    send_each(pts, 10000);
    unsigned long each = calls;
    printf("field by field : %6lu calls, %lu bytes\n", calls, bytes);
    calls = bytes = 0;
    send_batched(pts, 10000);
    printf("4 KiB buffer   : %6lu calls, %lu bytes\n", calls, bytes);
    printf("\nsame bytes, one call where there were %lu.\n", each / calls);
    puts("If one call costs a trip into the kernel, that is where the time goes.");
    puts("stdio's FILE does this for you; a raw socket does not.");
    return 0;
}
