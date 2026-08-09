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

    printf("셋이 확보한 크기는 같다: %zu, %zu, %zu 바이트\n",
           4 * sizeof *a, 4 * sizeof(struct node), 4 * sizeof *c);

    puts("\n[sizeof *p 는 p 를 읽지 않는다 — 평가되지 않는 피연산자]");
    struct node *nul = NULL;
    printf("  p 가 널이어도 sizeof *p = %zu 로 계산된다\n", sizeof *nul);
    puts("  크기는 *타입*에서 나오지 값에서 나오지 않기 때문이다.");

    puts("\n[이름을 두 번 적으면 어긋날 수 있다]");
    puts("  struct node *p = (struct node *)malloc(n * sizeof(struct gadget));");
    puts("  ↑ 타입 이름이 두 곳에 있으니 한쪽만 고치는 사고가 가능하다.");
    puts("  sizeof *p 로 적으면 그 사고를 *쓸 수가 없다*.");

    puts("\n[두 표기가 함께 지는 위험 — 곱셈의 넘침]");
    size_t huge = (size_t)-1 / sizeof(struct node) + 1;
    printf("  huge * sizeof *a 는 감아 돌아 %zu 가 된다\n", (size_t)(huge * sizeof *a));
    puts("  → malloc 이 '아주 작은' 크기로 성공할 수 있다. 개수를 먼저 검사하거나,");
    puts("     곱셈을 스스로 검사하는 calloc(개수, 크기) 를 쓴다.");

    free(a); free(b); free(c);
    return 0;
}
