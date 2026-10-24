/* malloc 을 적는 세 가지 표기 — 무엇이 다른가. */
#include <stdio.h>
#include <stdlib.h>

struct node { int id; double weight; };

int main(void)
{
    /* ── 권장 표기: 타입 이름이 한 번도 안 나온다 ── */
    struct node *a = malloc(4 * sizeof *a);
    if (!a) return 1;

    /* ── C++ 호환을 노린 표기: 타입 이름이 두 번 ── */
    struct node *b = (struct node *)malloc(4 * sizeof(struct node));
    if (!b) { free(a); return 1; }

    /* ── 절충: 캐스트는 붙이되 크기는 대상에서 가져온다 ── */
    struct node *c = (struct node *)malloc(4 * sizeof *c);
    if (!c) { free(a); free(b); return 1; }

    printf("all three reserve the same size: %zu, %zu, %zu bytes\n",
           4 * sizeof *a, 4 * sizeof(struct node), 4 * sizeof *c);

    puts("\n[sizeof *p does not read p - an unevaluated operand]");
    struct node *nul = NULL;
    printf("  even with p null, sizeof *p works out to %zu\n", sizeof *nul);
    puts("  the size comes from the *type*, not from the value.");

    puts("\n[write the name twice and the two can drift apart]");
    puts("  struct node *p = (struct node *)malloc(n * sizeof(struct gadget));");
    puts("  ^ the type name is in two places, so it is possible to fix only one.");
    puts("  written as sizeof *p, that mistake *cannot be written*.");

    puts("\n[a risk both spellings carry - overflow in the multiplication]");
    size_t huge = (size_t)-1 / sizeof(struct node) + 1;
    printf("  huge * sizeof *a wraps around to %zu\n", (size_t)(huge * sizeof *a));
    puts("  -> malloc can then succeed with a 'tiny' size. Check the count first, or");
    puts("     use calloc(count, size), which checks the multiplication itself.");

    free(a); free(b); free(c);
    return 0;
}
