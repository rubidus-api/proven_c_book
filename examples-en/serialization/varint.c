/* Variable-length integers (base-128, "LEB128"/protobuf style) and zigzag.
   Seven bits of payload per byte, low group first; the top bit says
   "another byte follows". Small numbers take few bytes. */
#include <stdint.h>
#include <stdio.h>

static size_t put_varint(unsigned char *out, uint64_t v)
{
    size_t n = 0;
    do {
        unsigned char b = v & 0x7F;
        v >>= 7;
        out[n++] = b | (v ? 0x80 : 0);      /* more to come? set the top bit */
    } while (v);
    return n;
}

/* Returns the number of bytes used, or 0 if the input is cut short or too long. */
static size_t get_varint(const unsigned char *in, size_t len, uint64_t *v)
{
    uint64_t x = 0;
    for (size_t i = 0; i < len && i < 10; i++) {          /* 64 bits need at most 10 */
        uint64_t part = in[i] & 0x7F;
        if (i == 9 && part > 1) return 0;                  /* would not fit in 64 bits */
        x |= part << (7 * i);
        if (!(in[i] & 0x80)) { *v = x; return i + 1; }
    }
    return 0;
}

/* zigzag: 0,-1,1,-2,2 ... -> 0,1,2,3,4 ... so small negatives stay small */
/* no right shift of a negative value: that is implementation-defined */
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
    unsigned char cut[]  = { 0xAC };              /* "more follows", then nothing */
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
