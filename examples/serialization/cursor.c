/* 끈적한 오류 표시를 단 쓰는 커서와 읽는 커서.
   모든 put/get 이 남은 자리를 확인한다. 한 번 확인에 걸리면 커서가 「bad」가 되고,
   그 뒤의 호출은 아무것도 하지 않는다. 부르는 쪽은 끝에서 한 번만 확인한다. */
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
    put_u8(w, pl->name_len);              /* 길이가 먼저 간다 */
    put_bytes(w, pl->name, pl->name_len);
}

static bool decode(struct rcur *r, struct player *pl)
{
    pl->id = get_u32le(r);
    pl->level = get_u16le(r);
    pl->name_len = get_u8(r);
    if (pl->name_len >= sizeof pl->name) r->bad = true;   /* 형식 자신의 상한 */
    const unsigned char *s = take(r, r->bad ? 0 : pl->name_len);
    if (r->bad) return false;             /* 확인은 한 번, 모든 읽기 뒤에 */
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

    r = (struct rcur){ .p = buf, .len = w.len - 2 };          /* 짧게 자른다 */
    printf("two bytes short : %s\n", decode(&r, &out) ? "ok" : "rejected");

    unsigned char lie[64];
    memcpy(lie, buf, w.len);
    lie[6] = 200;                                             /* 길이가 거짓말을 한다 */
    r = (struct rcur){ .p = lie, .len = w.len };
    printf("length says 200 : %s\n", decode(&r, &out) ? "ok" : "rejected");

    unsigned char tiny[8];
    w = (struct wcur){ .p = tiny, .cap = sizeof tiny };
    encode(&w, &in);
    printf("8-byte buffer   : writer is %s after %zu bytes\n",
           w.bad ? "bad" : "fine", w.len);
    return 0;
}
