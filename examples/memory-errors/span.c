/* 길이를 데리고 다니는 버퍼 --- 오버플로를 *구조로* 막는 한 가지 모양.
 * 포인터 하나만 넘기면 받는 쪽은 그릇의 크기를 모른다(strcpy 의 문제).
 * 포인터와 길이를 한 묶음으로 넘기면, 모든 쓰기가 같은 자리에서 검사된다. */
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char  *data;
    size_t len;     /* 쓴 바이트 수 */
    size_t cap;     /* 그릇의 크기 */
} buf;

/* 들어갈 자리가 없으면 그릇을 키운다. 실패하면 false --- 옛 내용은 그대로 남는다. */
static bool reserve(buf *b, size_t extra)
{
    size_t need;
    if (ckd_add(&need, b->len, extra)) return false;    /* 길이 계산이 넘치면 거절 */
    if (need <= b->cap) return true;

    size_t cap = b->cap ? b->cap : 8;
    while (cap < need)
        if (ckd_mul(&cap, cap, 2)) return false;

    char *p = realloc(b->data, cap);    /* 결과를 먼저 다른 변수에 받는다 */
    if (p == NULL) return false;        /* 실패해도 b->data 는 살아 있다(C23 7.24.3.7p3) */
    b->data = p;
    b->cap  = cap;
    return true;
}

static bool append(buf *b, const char *s)
{
    size_t n = strlen(s);
    if (!reserve(b, n)) return false;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return true;
}

/* n 개짜리 배열을 빌릴 때의 크기 계산 --- n * size 가 넘치면 작은 블록이 돌아온다. */
static void *alloc_array(size_t n, size_t size)
{
    size_t bytes;
    if (ckd_mul(&bytes, n, size)) return NULL;
    return malloc(bytes);
}

int main(void)
{
    buf b = {0};
    const char *words[] = {"length ", "travels ", "with ", "the ", "pointer"};
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++)
        if (!append(&b, words[i])) { free(b.data); return 1; }
    printf("%.*s\n", (int)b.len, b.data);
    printf("len %zu, cap %zu\n", b.len, b.cap);
    free(b.data);

    void *huge = alloc_array(SIZE_MAX / 2, 4);  /* 곱이 size_t 를 넘친다 */
    printf("alloc_array(SIZE_MAX / 2, 4): %s\n", huge ? "got a block" : "refused (size overflow)");
    free(huge);
    return 0;
}
