/* 참조 계수 --- 「나를 가리키는 것이 몇인가」를 세어, 0 이 되는 순간 돌려준다.
 * 그리고 그 약점: 서로 가리키는 둘(순환)은 계수가 끝내 0 이 되지 않는다.
 * 한쪽을 *약한 참조*(계수를 올리지 않는 참조)로 바꾸면 순환이 풀린다. */
#include <stdio.h>
#include <stdlib.h>

typedef struct node node;
struct node {
    const char *name;
    int   refs;
    node *strong;       /* 계수를 올리는 참조 */
    node *weak;         /* 계수를 올리지 않는 참조 --- 소유하지 않는다 */
};

static int live;        /* 아직 돌려주지 않은 노드 수 */

static node *node_new(const char *name)
{
    node *n = calloc(1, sizeof *n);
    if (n == NULL) exit(1);
    n->name = name;
    n->refs = 1;        /* 만든 쪽이 첫 참조를 쥔다 */
    live++;
    return n;
}

static node *retain(node *n) { if (n) n->refs++; return n; }

static void release(node *n)
{
    if (n == NULL || --n->refs > 0) return;
    printf("  free %s\n", n->name);
    release(n->strong);             /* 내가 쥔 강한 참조를 놓는다 */
    free(n);
    live--;
}

int main(void)
{
    puts("cycle of two strong references:");
    node *a = node_new("a"), *b = node_new("b");
    a->strong = retain(b);
    b->strong = retain(a);          /* a -> b -> a */
    release(a);
    release(b);                     /* 바깥 참조를 다 놓았다 */
    printf("  still alive: %d (a.refs %d, b.refs %d) --- leaked\n", live, a->refs, b->refs);

    /* 시연이 끝났으니 손으로 끊어 정리한다 --- 수집기가 없으면 이 일은 사람 몫이다. */
    a->strong = NULL; b->strong = NULL;
    free(a); free(b); live -= 2;

    puts("same shape, back edge made weak:");
    node *c = node_new("c"), *d = node_new("d");
    c->strong = retain(d);
    d->weak = c;                    /* d -> c 는 계수를 올리지 않는다 */
    release(c);
    release(d);
    printf("  still alive: %d\n", live);
    return 0;
}
