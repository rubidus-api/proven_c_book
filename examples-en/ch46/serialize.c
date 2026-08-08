/* What goes out when a struct is written whole — and how to serialise properly. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct record {
    uint8_t  kind;      /* 1 byte */
    uint32_t id;        /* 4 bytes — must sit at a multiple of four */
    uint16_t flags;     /* 2 bytes */
};

static void dump(const char *label, const unsigned char *b, size_t n)
{
    printf("  %-16s", label);
    for (size_t i = 0; i < n; i++) printf(" %02X", b[i]);
    printf("   (%zu bytes)\n", n);
}

/* ── The right way: you decide the byte order ──────────────────────
   Write fixed-width types one at a time in a fixed order (big endian here).
   No padding can creep in, and any machine reads the same meaning. */
static size_t put_u8(unsigned char *p, uint8_t v)  { p[0] = v; return 1; }
static size_t put_u16(unsigned char *p, uint16_t v)
{ p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; return 2; }
static size_t put_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
    return 4;
}

static size_t encode(unsigned char *out, const struct record *r)
{
    size_t n = 0;
    n += put_u8(out + n, r->kind);
    n += put_u32(out + n, r->id);
    n += put_u16(out + n, r->flags);
    return n;
}

static int decode(const unsigned char *in, size_t len, struct record *r)
{
    if (len < 7) return 0;                       /* validate the length first */
    r->kind  = in[0];
    r->id    = (uint32_t)in[1] << 24 | (uint32_t)in[2] << 16
             | (uint32_t)in[3] << 8  | (uint32_t)in[4];
    r->flags = (uint16_t)((uint16_t)in[5] << 8 | in[6]);
    return 1;
}

int main(void)
{
    printf("struct record: sizeof = %zu (members sum to %zu)\n\n",
           sizeof(struct record),
           sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t));

    /* Imitate a place that still holds whatever was there before —
       a struct on the stack really does land on somebody else's leftovers. */
    struct record r;
    memset(&r, 0xAA, sizeof r);      /* dirty the place, then */
    r.kind = 1; r.id = 0x01020304; r.flags = 0x0506;   /* fill only the members */

    puts("[written whole]");
    dump("bytes", (const unsigned char *)&r, sizeof r);
    puts("  -> where AA shows is padding: never given a value, and out it goes.");
    puts("    (imitated here, but real uninitialised values leak the same way)");

    puts("\n[serialised field by field]");
    unsigned char buf[16];
    size_t n = encode(buf, &r);
    dump("bytes", buf, n);
    puts("  -> no padding, and the order is the one we chose (big endian).");

    struct record back;
    if (decode(buf, n, &back))
        printf("  read back: kind=%u id=0x%08X flags=0x%04X — %s\n",
               back.kind, back.id, back.flags,
               (back.kind == r.kind && back.id == r.id && back.flags == r.flags)
               ? "same as the original" : "different");

    puts("\n[why comparing whole structs is dangerous]");
    struct record a, b;
    memset(&a, 0x00, sizeof a);
    memset(&b, 0xFF, sizeof b);      /* only the padding differs */
    a.kind = b.kind = 1; a.id = b.id = 42; a.flags = b.flags = 7;
    printf("  every member is equal. memcmp = %d  <- non-zero means \"different\"\n",
           memcmp(&a, &b, sizeof a) != 0 ? 1 : 0);
    puts("  So compare structs member by member.");
    return 0;
}
