/* 작은 점검용 할당자 --- 빌린 블록마다 장부에 적는다.
 * 돌려받은 블록은 곧바로 풀지 않고 *격리*해 둔다(독 0xDD 를 칠해서). 그래서 두 번째
 * 해제가 와도 그 주소는 아직 살아 있는 객체를 가리키고, 장부와 견줘 볼 수 있다.
 * 진짜로 풀어 버렸다면 그 포인터 값을 쓰는 것부터가 계약 밖이다(C23 6.2.4p2). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_BLOCKS = 16 };

typedef struct {
    unsigned char *p;
    size_t size;
    const char *where;
    bool released;          /* 돌려받았으나 격리 중 */
} entry;
static entry book[MAX_BLOCKS];

static void *chk_malloc(size_t size, const char *where)
{
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        if (book[i].p == NULL) {
            unsigned char *p = malloc(size);
            if (p == NULL) return NULL;
            book[i] = (entry){ p, size, where, false };
            return p;
        }
    }
    return NULL;                        /* 장부가 찼다 */
}

static void chk_free(void *ptr, const char *where)
{
    if (ptr == NULL) return;
    unsigned char *p = ptr;
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        entry *e = &book[i];
        if (e->p == NULL) continue;
        if (p == e->p) {
            if (e->released) {
                printf("  %s: double free of the block from %s\n", where, e->where);
            } else {
                memset(e->p, 0xDD, e->size);    /* 독 칠하기 */
                e->released = true;             /* 격리: 아직 free 하지 않는다 */
            }
            return;
        }
        /* 「이 블록 안인가」는 포인터의 < 로 묻지 않는다 --- 서로 다른 객체의 포인터를
           크기로 견주는 것은 계약 밖이다(6.5.9). 정수로 바꾼 주소로 묻는다(구현 정의). */
        uintptr_t a = (uintptr_t)p, lo = (uintptr_t)e->p;
        if (a > lo && a - lo < e->size) {
            printf("  %s: free of an address inside the block from %s (offset %ju)\n",
                   where, e->where, (uintmax_t)(a - lo));
            return;
        }
    }
    printf("  %s: free of a pointer this allocator never gave out\n", where);
}

/* 끝에서: 돌려받지 못한 것은 누수로 알리고, 격리한 것까지 모두 실제로 푼다. */
static void chk_finish(void)
{
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        if (book[i].p == NULL) continue;
        if (!book[i].released)
            printf("  leak: %zu bytes from %s\n", book[i].size, book[i].where);
        free(book[i].p);
        book[i] = (entry){0};
    }
}

int main(void)
{
    char *name = chk_malloc(16, "line A");
    char *tmp  = chk_malloc(64, "line B");
    int  *nums = chk_malloc(4 * sizeof *nums, "line C");
    if (!name || !tmp || !nums) return 1;

    strcpy(name, "ok");
    chk_free(tmp, "line D");
    chk_free(tmp, "line E");            /* 두 번째 해제 */
    chk_free(name + 1, "line F");       /* 블록의 가운데 */
    chk_free(name, "line G");
    printf("  first byte of the released block from line B: 0x%02X\n",
           (unsigned)(unsigned char)tmp[0]);    /* 격리 중이라 읽어도 계약 안이다 */

    puts("at exit:");
    chk_finish();                       /* nums 는 돌려주지 않았다 */
    return 0;
}
