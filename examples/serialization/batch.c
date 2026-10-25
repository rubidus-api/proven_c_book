/* 필드마다 쓰기와, 먼저 버퍼 하나에 모으기.
   `sink` 는 호출마다 비싼 것을 대신한다 --- 소켓의 send(), 파일의 write(), USB 전송.
   여기서는 세기만 한다. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned long calls, bytes;
static void sink(const void *p, size_t n) { (void)p; calls++; bytes += n; }

struct point { int32_t x, y; uint16_t id; };

static void put_le(unsigned char *b, uint32_t v, int n)
{ for (int i = 0; i < n; i++) b[i] = (unsigned char)(v >> (8 * i)); }

/* (1) 필드마다 따로 나간다 */
static void send_each(const struct point *pts, size_t count)
{
    unsigned char b[4];
    for (size_t i = 0; i < count; i++) {
        put_le(b, (uint32_t)pts[i].x, 4);  sink(b, 4);
        put_le(b, (uint32_t)pts[i].y, 4);  sink(b, 4);
        put_le(b, pts[i].id, 2);           sink(b, 2);
    }
}

/* (2) 버퍼를 채우고, 차면 보내고, 끝에 한 번 더 */
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
