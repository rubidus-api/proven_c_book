/* 장난감 표시-쓸기 수집기 --- 「뿌리에서 닿을 수 있는가」로 살고 죽음을 가른다.
 * 참조 계수가 못 거둔 순환도, 뿌리에서 끊기면 거둔다. 실제 수집기와 달리 뿌리를
 * 손으로 등록한다 --- C 에는 「어디가 포인터인가」를 알려 주는 장부가 없기 때문이다. */
#include <stdio.h>
#include <stdlib.h>

typedef struct obj obj;
struct obj {
    char  name;
    obj  *ref[2];       /* 다른 객체를 가리키는 칸 둘 */
    bool  marked;
    int   survived;     /* 수집을 몇 번 살아남았나 */
    obj  *next_alloc;   /* 수집기가 아는 모든 객체의 목록 */
};

static obj *all;                /* 할당한 모든 객체 */
static obj *roots[4];           /* 뿌리: 프로그램이 지금 쥐고 있는 것 */

static obj *gc_new(char name)
{
    obj *o = calloc(1, sizeof *o);
    if (o == NULL) exit(1);
    o->name = name;
    o->next_alloc = all;
    all = o;
    return o;
}

static void mark(obj *o)
{
    obj *stack[64];             /* 재귀 대신 명시적 스택 --- 깊은 그래프에서 호출 스택을 넘치지 않게 */
    size_t top = 0;
    if (o) stack[top++] = o;
    while (top > 0) {
        obj *x = stack[--top];
        if (x->marked) continue;        /* 순환이어도 한 번만 표시한다 */
        x->marked = true;
        for (int i = 0; i < 2; i++)
            if (x->ref[i] && !x->ref[i]->marked && top < sizeof stack / sizeof stack[0])
                stack[top++] = x->ref[i];
    }
}

static void collect(void)
{
    for (size_t i = 0; i < sizeof roots / sizeof roots[0]; i++)
        mark(roots[i]);                 /* ① 표시: 뿌리에서 닿는 것 */

    printf("  freed:");
    for (obj **p = &all; *p != NULL; ) { /* ② 쓸기: 표시가 없는 것을 돌려준다 */
        obj *o = *p;
        if (!o->marked) {
            *p = o->next_alloc;
            printf(" %c", o->name);
            free(o);
        } else {
            o->marked = false;          /* 다음 수집을 위해 지운다 */
            o->survived++;
            p = &o->next_alloc;
        }
    }
    printf("\n  alive:");
    for (obj *o = all; o != NULL; o = o->next_alloc)
        printf(" %c(survived %d)", o->name, o->survived);
    printf("\n");
}

int main(void)
{
    obj *a = gc_new('a'), *b = gc_new('b'), *c = gc_new('c');
    a->ref[0] = b;
    b->ref[0] = c;
    c->ref[0] = b;                      /* b <-> c 순환 */
    roots[0] = a;
    (void)gc_new('t');                  /* 아무도 쥐지 않는 임시 객체 */

    puts("collection 1 (root: a):");
    collect();

    a->ref[0] = NULL;                   /* a 에서 순환으로 가는 길을 끊는다 */
    (void)gc_new('u');
    puts("collection 2 (a no longer points at the cycle):");
    collect();

    roots[0] = NULL;                    /* 뿌리를 비우고 마지막 수집 */
    puts("collection 3 (no roots):");
    collect();
    return 0;
}
