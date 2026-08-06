#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }

/* 함수 포인터를 받는 함수 — 콜백의 기본형 */
static int apply(int (*op)(int, int), int a, int b) { return op(a, b); }

int main(void)
{
    /* ① 함수 이름은 값으로 쓰이는 순간 포인터로 무너진다 */
    int (*p)(int, int) = add;     /* & 없이도 된다 */
    int (*q)(int, int) = &add;    /* & 를 붙여도 같다 */
    printf("add == &add : %s\n", p == q ? "같다" : "다르다");

    /* ② 별표를 아무리 붙여도 결과는 같다 — 역참조가 다시 무너지기 때문 */
    printf("p(2,3)=%d  (*p)(2,3)=%d  (***p)(2,3)=%d  (*******p)(2,3)=%d\n",
           p(2, 3), (*p)(2, 3), (***p)(2, 3), (*******p)(2, 3));

    /* ③ 반면 & 는 한 번만 쓸 수 있다: &&add 는 문법 오류다
          (&add 는 값일 뿐, 그 값의 주소를 다시 얻을 수는 없다) */

    /* ④ 디스패치 표 — switch 대신 배열로 고르기 */
    struct { const char *name; int (*fn)(int, int); } table[] = {
        { "add", add }, { "mul", mul },
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        printf("%s(6,7) = %d\n", table[i].name, apply(table[i].fn, 6, 7));

    /* ⑤ 함수 포인터의 크기는 데이터 포인터와 같다는 보장이 없다 */
    printf("sizeof(void*)=%zu, sizeof(int(*)(int,int))=%zu\n",
           sizeof(void *), sizeof(int (*)(int, int)));

    /* ⑥ qsort 의 비교자도 함수 포인터다 */
    int v[] = { 5, 2, 9, 1 };
    int cmp(const void *a, const void *b);   /* 아래에 정의 */
    qsort(v, 4, sizeof v[0], cmp);
    printf("정렬: %d %d %d %d\n", v[0], v[1], v[2], v[3]);
    return 0;
}

int cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}
