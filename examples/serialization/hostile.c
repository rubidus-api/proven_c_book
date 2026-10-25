/* 적의를 품은 입력이 할 수 있는 거짓말 셋과, 그것을 거절하는 확인.
   여기서는 아무것도 넘치지 않는다: 확인하지 않은 쪽은 계산해서 인쇄만 한다. */
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* (1) 머리말이 너비 × 높이 × 4 바이트라고 한다. 32비트로 곱하면 넘쳐 되돈다. */
static void image_size(uint32_t w, uint32_t h)
{
    uint32_t naive = w * h * 4u;      /* 부호 없음: 되돌 뿐 UB 아님 --- 경고도 없다 */
    size_t checked;
    bool over = ckd_mul(&checked, (size_t)w, (size_t)h)
             || ckd_mul(&checked, checked, (size_t)4);
    printf("  %10u x %-10u naive 32-bit size %10u",
           (unsigned)w, (unsigned)h, (unsigned)naive);
    if (over) puts("   checked: refused (does not fit in size_t)");
    else if (checked > 256u << 20)
        printf("   checked: %zu, refused (over the 256 MiB limit)\n", checked);
    else printf("   checked: %zu, accepted\n", checked);
}

/* (2) 개수가 「항목 10억 개가 뒤따른다」고 하는데 입력에는 몇 바이트뿐이다.
   할당하기 *전에* 그 주장을 실제로 있는 것과 견준다. */
static void read_items(uint32_t claimed, size_t bytes_left, size_t item_size)
{
    printf("  claims %10u items, %3zu bytes left: ", (unsigned)claimed, bytes_left);
    if (claimed > bytes_left / item_size) { puts("refused before malloc"); return; }
    void *p = malloc((size_t)claimed * item_size);
    puts(p ? "allocated" : "allocation failed");
    free(p);
}

/* (3) 중첩: 0x01 은 목록을 열고 0x02 는 닫는다. 재귀로 읽는 쪽에는
   깊이 상한이 필요하다 --- 없으면 0x01 이 길게 이어진 입력이 스택을 다 쓴다. */
enum { MAX_DEPTH = 32 };
static const unsigned char *parse_list(const unsigned char *p,
                                       const unsigned char *end, int depth, bool *ok)
{
    if (depth > MAX_DEPTH) { *ok = false; return end; }
    while (p < end && *ok) {
        if (*p == 0x01) p = parse_list(p + 1, end, depth + 1, ok);
        else if (*p == 0x02) return p + 1;
        else p++;
    }
    return p;
}

int main(void)
{
    puts("[1] sizes from a header");
    image_size(640, 480);
    image_size(65536, 16385);
    image_size(0x40000000u, 4);
    image_size(UINT32_MAX, UINT32_MAX);

    puts("\n[2] counts from a header (8-byte items)");
    read_items(3, 24, 8);
    read_items(1000000000u, 24, 8);

    puts("\n[3] nesting depth");
    static unsigned char deep[100000];
    for (size_t i = 0; i < sizeof deep; i++) deep[i] = 0x01;
    unsigned char fine[] = { 0x01, 0x01, 0x07, 0x02, 0x02 };
    bool ok = true;
    parse_list(fine, fine + sizeof fine, 0, &ok);
    printf("  5 bytes, depth 2       : %s\n", ok ? "accepted" : "refused");
    ok = true;
    parse_list(deep, deep + sizeof deep, 0, &ok);
    printf("  100000 opens in a row  : %s at depth %d\n",
           ok ? "accepted" : "refused", MAX_DEPTH + 1);
    return 0;
}
