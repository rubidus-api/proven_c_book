#include <proven.h>
#include <stdio.h>

static void dump(const char *label, proven_mem_view_t v)
{
    printf("%-10s size=%zu bytes:", label, v.size);
    for (proven_size_t i = 0; i < v.size; i++) printf(" %02x", v.ptr[i]);
    printf("\n");
}

int main(void)
{
    /* 원시 바이트는 proven_byte_t 다 — unsigned char 의 별칭이라
       어떤 객체의 표현이든 들여다볼 수 있는 유일한 타입이다 */
    proven_byte_t buf[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};

    /* view = 포인터 + 길이. 길이를 따로 들고 다니지 않는다 */
    proven_mem_view_t all = { .ptr = buf, .size = sizeof buf };
    dump("all", all);

    /* 자르기: 경계를 넘으면 실패가 값으로 온다 */
    proven_result_mem_view_t mid = proven_mem_view_slice_checked(all, 2, 3);
    if (proven_is_ok(mid.err)) dump("slice 2+3", mid.value);

    proven_result_mem_view_t over = proven_mem_view_slice_checked(all, 6, 4);
    printf("%-10s err=%d (refused, not clamped)\n", "slice 6+4", (int)over.err);

    /* 크기 계산도 감기지 않게 한다: 배열 원소 수 x 원소 크기 */
    proven_size_t n = (proven_size_t)-1 / 2;   /* 터무니없이 큰 개수 */
    proven_size_t bytes;
    if (PROVEN_CKD_MUL(&bytes, n, (proven_size_t)8))
        printf("%-10s %zu * 8 overflows — allocation refused\n", "size calc", n);
    else
        printf("%-10s %zu bytes\n", "size calc", bytes);

    /* 정렬 올림: 다음 경계로 밀어 올린다 */
    printf("align_up(13, 8) = %zu\n", proven_mem_align_up(13, 8));
    return 0;
}
