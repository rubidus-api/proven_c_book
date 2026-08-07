/* 구조체를 통째로 저장하면 무엇이 함께 나가는가 — 그리고 올바른 직렬화. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct record {
    uint8_t  kind;      /* 1바이트 */
    uint32_t id;        /* 4바이트 — 4의 배수 자리에 놓여야 한다 */
    uint16_t flags;     /* 2바이트 */
};

static void dump(const char *label, const unsigned char *b, size_t n)
{
    printf("  %-16s", label);
    for (size_t i = 0; i < n; i++) printf(" %02X", b[i]);
    printf("   (%zu바이트)\n", n);
}

/* ── 올바른 방법: 바이트 순서를 내가 정한다 ─────────────────────
   고정 폭 타입을 하나씩, 정해진 순서(여기서는 빅 엔디안)로 적는다.
   패딩이 끼어들 자리가 없고, 어느 기계에서 읽어도 같은 뜻이 된다. */
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
    if (len < 7) return 0;                       /* 길이부터 검증한다 */
    r->kind  = in[0];
    r->id    = (uint32_t)in[1] << 24 | (uint32_t)in[2] << 16
             | (uint32_t)in[3] << 8  | (uint32_t)in[4];
    r->flags = (uint16_t)((uint16_t)in[5] << 8 | in[6]);
    return 1;
}

int main(void)
{
    printf("struct record: sizeof = %zu (멤버 합은 %zu)\n\n",
           sizeof(struct record),
           sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t));

    /* 이 자리를 쓰던 값이 남아 있는 상황을 흉내 낸다 —
       스택의 구조체는 실제로 이렇게 '앞사람의 쓰레기' 위에 놓인다. */
    struct record r;
    memset(&r, 0xAA, sizeof r);      /* 자리를 더럽혀 두고 */
    r.kind = 1; r.id = 0x01020304; r.flags = 0x0506;   /* 멤버만 채운다 */

    puts("[통째로 내보내면]");
    dump("바이트", (const unsigned char *)&r, sizeof r);
    puts("  → AA 가 보이는 자리가 패딩이다. 값을 정한 적이 없는데 함께 나간다.");
    puts("    (여기서는 흉내 냈지만, 실제로도 초기화되지 않은 값이 새어 나간다)");

    puts("\n[필드별로 직렬화하면]");
    unsigned char buf[16];
    size_t n = encode(buf, &r);
    dump("바이트", buf, n);
    puts("  → 패딩이 없고, 순서를 내가 정했다(여기서는 빅 엔디안).");

    struct record back;
    if (decode(buf, n, &back))
        printf("  다시 읽으면: kind=%u id=0x%08X flags=0x%04X — %s\n",
               back.kind, back.id, back.flags,
               (back.kind == r.kind && back.id == r.id && back.flags == r.flags)
               ? "원본과 같다" : "다르다");

    puts("\n[같은 값인데 통째 비교는 왜 위험한가]");
    struct record a, b;
    memset(&a, 0x00, sizeof a);
    memset(&b, 0xFF, sizeof b);      /* 패딩만 다르게 */
    a.kind = b.kind = 1; a.id = b.id = 42; a.flags = b.flags = 7;
    printf("  멤버는 모두 같다. memcmp = %d  ← 0 이 아니면 '다르다'는 뜻\n",
           memcmp(&a, &b, sizeof a) != 0 ? 1 : 0);
    puts("  그래서 구조체 비교는 멤버별로 한다.");
    return 0;
}
