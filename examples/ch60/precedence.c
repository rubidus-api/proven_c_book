/* 우경식(牛耕式) 읽기의 우선순위 규칙을 눈으로 확인한다.
   특히 const/volatile 이 「왼쪽의 별표에 붙는가, 타입에 붙는가」를 실물로 가른다. */
#include <stdio.h>

static int one = 1, two = 2;

int main(void)
{
    /* ① const 가 타입 지정자 옆에 있다 → 타입에 붙는다(가리키는 것이 읽기 전용) */
    const int *pci  = &one;     /* 「int const 를 가리키는 포인터」 */
    int const *pci2 = &one;     /* 위와 완전히 같다 — 순서만 다르다 */

    /* ② const 가 타입 지정자 옆이 아니다 → 바로 왼쪽의 별표에 붙는다 */
    int *const cpi = &one;      /* 「int 를 가리키는, 읽기 전용 포인터」 */

    /* ③ 둘 다 */
    const int *const cpci = &one;

    /* ①은 가리키는 곳을 바꿀 수 있다.  *pci = 9; 는 오류다. */
    pci = &two;
    pci2 = &two;
    printf("(1) *pci=%d  *pci2=%d   <- 포인터 자신은 옮길 수 있다\n", *pci, *pci2);

    /* ②는 반대다.  cpi = &two; 는 오류이고, 가리키는 값은 바꿀 수 있다. */
    *cpi = 42;
    printf("(2) *cpi=%d   one=%d      <- 값은 바꿀 수 있다\n", *cpi, one);

    printf("(3) *cpci=%d              <- 양쪽 다 읽기 전용\n", *cpci);

    /* ④ 우경식으로 읽는 실물: 오른쪽 먼저, 막히면 왼쪽 */
    char *const *next = NULL;   /* next 는 「char 를 가리키는 읽기 전용 포인터」를
                                   가리키는 포인터 */
    printf("(4) sizeof next = %zu   (포인터 하나)\n", sizeof next);

    /* ⑤ 태그를 붙여 두었기에 자기 자신을 가리킬 수 있다 */
    struct node_tag { int datum; struct node_tag *next; };
    struct node_tag b = { 2, NULL };
    struct node_tag a = { 1, &b };
    printf("(5) a.datum=%d -> a.next->datum=%d\n", a.datum, a.next->datum);

    return 0;
}
