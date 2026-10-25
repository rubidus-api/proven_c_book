/* 가변 길이 정수(128진, 「LEB128」·프로토콜 버퍼 방식)와 지그재그.
   바이트마다 값 7비트, 낮은 묶음이 먼저. 맨 위 비트는 「뒤에 바이트가 더 있다」.
   작은 수는 바이트를 적게 쓴다. */
#include <stdint.h>
#include <stdio.h>

static size_t put_varint(unsigned char *out, uint64_t v)
{
    size_t n = 0;
    do {
        unsigned char b = v & 0x7F;
        v >>= 7;
        out[n++] = b | (v ? 0x80 : 0);      /* 더 있는가? 맨 위 비트를 켠다 */
    } while (v);
    return n;
}

/* 쓴 바이트 수를 돌려준다. 입력이 잘렸거나 너무 길면 0. */
static size_t get_varint(const unsigned char *in, size_t len, uint64_t *v)
{
    uint64_t x = 0;
    for (size_t i = 0; i < len && i < 10; i++) {          /* 64비트는 많아야 10바이트 */
        uint64_t part = in[i] & 0x7F;
        if (i == 9 && part > 1) return 0;                  /* 64비트에 들어가지 않는다 */
        x |= part << (7 * i);
        if (!(in[i] & 0x80)) { *v = x; return i + 1; }
    }
    return 0;
}

/* 지그재그: 0,-1,1,-2,2 … -> 0,1,2,3,4 … 작은 음수도 작게 남는다 */
/* 음수를 오른쪽으로 밀지 않는다: 그것은 구현 정의다 */
static uint64_t zigzag(int64_t n)
{ uint64_t u = (uint64_t)n; return (u << 1) ^ (0 - (u >> 63)); }
static int64_t  unzigzag(uint64_t u) { return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }

static void show(const char *label, uint64_t v)
{
    unsigned char b[10];
    size_t n = put_varint(b, v);
    printf("  %-22s ->", label);
    for (size_t i = 0; i < n; i++) printf(" %02X", b[i]);
    printf("   (%zu byte%s)\n", n, n == 1 ? "" : "s");
}

int main(void)
{
    puts("[varint]");
    show("1", 1);
    show("127", 127);
    show("128", 128);
    show("150", 150);
    show("300", 300);
    show("UINT32_MAX", UINT32_MAX);
    show("-1 as uint64_t", (uint64_t)-1);

    puts("\n[zigzag, then varint]");
    for (int64_t n = -2; n <= 2; n++) {
        char label[32];
        snprintf(label, sizeof label, "%lld -> %llu", (long long)n,
                 (unsigned long long)zigzag(n));
        show(label, zigzag(n));
    }

    puts("\n[reading back]");
    unsigned char good[] = { 0xAC, 0x02 };
    unsigned char cut[]  = { 0xAC };              /* 「더 있다」 하고 아무것도 없다 */
    unsigned char runaway[11] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01 };
    uint64_t v = 0;
    size_t n = get_varint(good, sizeof good, &v);
    printf("  AC 02            -> %llu (%zu bytes)\n", (unsigned long long)v, n);
    printf("  AC               -> %s\n",
           get_varint(cut, sizeof cut, &v) ? "ok" : "rejected: cut short");
    printf("  FF x10, 01       -> %s\n",
           get_varint(runaway, sizeof runaway, &v) ? "ok" : "rejected: too long");
    printf("  unzigzag(3)      -> %lld\n", (long long)unzigzag(3));
    return 0;
}
