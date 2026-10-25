/* A writing cursor and a reading cursor with a sticky error flag.
   Every put/get checks the room left; once a check fails the cursor turns
   "bad" and every later call does nothing. The caller checks once, at the end. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct wcur { unsigned char *p; size_t cap, len; bool bad; };
struct rcur { const unsigned char *p; size_t len, pos; bool bad; };

static void put_bytes(struct wcur *w, const void *src, size_t n)
{
    if (w->bad || n > w->cap - w->len) { w->bad = true; return; }
    memcpy(w->p + w->len, src, n);
    w->len += n;
}
static void put_u8(struct wcur *w, uint8_t v) { put_bytes(w, &v, 1); }
static void put_u16le(struct wcur *w, uint16_t v)
{
    unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) };
    put_bytes(w, b, 2);
}
static void put_u32le(struct wcur *w, uint32_t v)
{
    unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8),
                           (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    put_bytes(w, b, 4);
}

static const unsigned char *take(struct rcur *r, size_t n)
{
    if (r->bad || n > r->len - r->pos) { r->bad = true; return nullptr; }
    const unsigned char *at = r->p + r->pos;
    r->pos += n;
    return at;
}
static uint8_t get_u8(struct rcur *r)
{ const unsigned char *b = take(r, 1); return b ? b[0] : 0; }
static uint16_t get_u16le(struct rcur *r)
{ const unsigned char *b = take(r, 2); return b ? (uint16_t)(b[0] | b[1] << 8) : 0; }
static uint32_t get_u32le(struct rcur *r)
{
    const unsigned char *b = take(r, 4);
    return b ? (uint32_t)b[0] | (uint32_t)b[1] << 8
             | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24 : 0;
}

struct player { uint32_t id; uint16_t level; uint8_t name_len; char name[32]; };

static void encode(struct wcur *w, const struct player *pl)
{
    put_u32le(w, pl->id);
    put_u16le(w, pl->level);
    put_u8(w, pl->name_len);              /* the length goes first */
    put_bytes(w, pl->name, pl->name_len);
}

static bool decode(struct rcur *r, struct player *pl)
{
    pl->id = get_u32le(r);
    pl->level = get_u16le(r);
    pl->name_len = get_u8(r);
    if (pl->name_len >= sizeof pl->name) r->bad = true;   /* the format's own limit */
    const unsigned char *s = take(r, r->bad ? 0 : pl->name_len);
    if (r->bad) return false;             /* one check, after all the reads */
    memcpy(pl->name, s, pl->name_len);
    pl->name[pl->name_len] = '\0';
    return true;
}

int main(void)
{
    struct player in = { .id = 0x01020304, .level = 7, .name_len = 5, .name = "alice" };
    unsigned char buf[64];
    struct wcur w = { .p = buf, .cap = sizeof buf };
    encode(&w, &in);
    printf("encoded %zu bytes:", w.len);
    for (size_t i = 0; i < w.len; i++) printf(" %02X", buf[i]);
    printf("\n  (sizeof(struct player) is %zu ---"
           " the bytes are the format, not the struct)\n\n",
           sizeof(struct player));

    struct player out;
    struct rcur r = { .p = buf, .len = w.len };
    printf("whole message   : %s", decode(&r, &out) ? "ok" : "rejected");
    printf("  -> id %08X, level %u, name \"%s\"\n",
           (unsigned)out.id, out.level, out.name);

    r = (struct rcur){ .p = buf, .len = w.len - 2 };          /* cut short */
    printf("two bytes short : %s\n", decode(&r, &out) ? "ok" : "rejected");

    unsigned char lie[64];
    memcpy(lie, buf, w.len);
    lie[6] = 200;                                             /* the length lies */
    r = (struct rcur){ .p = lie, .len = w.len };
    printf("length says 200 : %s\n", decode(&r, &out) ? "ok" : "rejected");

    unsigned char tiny[8];
    w = (struct wcur){ .p = tiny, .cap = sizeof tiny };
    encode(&w, &in);
    printf("8-byte buffer   : writer is %s after %zu bytes\n",
           w.bad ? "bad" : "fine", w.len);
    return 0;
}
