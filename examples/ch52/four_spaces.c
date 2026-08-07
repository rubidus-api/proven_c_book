/* 같은 철자 x 를 네 이름 공간에 동시에 둔다 — 표준 §6.2.3.
   합법이고 컴파일된다. 읽기 좋은가는 별개의 문제다. */
#include <stdio.h>

/* ② 태그 이름 공간 — struct/union/enum 뒤에 오는 이름 */
struct x {
    int x;        /* ③ 멤버 이름 공간 — struct x 만의 마당 */
    int y;
};

/* 다른 구조체의 멤버는 또 다른 마당이라 같은 철자를 다시 써도 된다 */
struct point { int x; int y; };

/* ④ 보통 식별자 — 변수·함수·typedef 이름·열거 상수가 모두 여기에 산다.
      태그 x 와 철자가 같지만 다른 이름 공간이라 공존한다. */
typedef struct x x;

static int show(x v, struct point p)
{
    /* ① 레이블 이름 공간 — goto 의 표적. 이것도 x 라 지을 수 있다 */
    if (v.x < 0) goto x;

    printf("struct x  : x=%d y=%d\n", v.x, v.y);
    printf("struct point: x=%d y=%d\n", p.x, p.y);
    return 0;

x:  /* 레이블 x */
    puts("음수라 레이블 x 로 왔다");
    return 1;
}

int main(void)
{
    /* 네 이름 공간에 x 가 하나씩 있는 상태에서 전부 쓴다 */
    x       a = { .x = 10, .y = 20 };   /* typedef 이름 x */
    struct x b = { .x = -1, .y = 0 };   /* 태그 x */
    struct point p = { .x = 3, .y = 4 };

    puts("[네 이름 공간에 같은 철자 x 가 동시에 산다]");
    puts("  ① 레이블 x   ② 태그 struct x   ③ 멤버 x   ④ typedef 이름 x");
    puts("");

    (void)show(a, p);
    (void)show(b, p);

    /* 이름을 찾는 자리(문법적 문맥)가 이름 공간을 고른다.
       struct 뒤 → 태그, . 뒤 → 멤버, goto 뒤 → 레이블, 그 밖 → 보통 식별자. */
    printf("\nsizeof(x) = %zu, sizeof(struct x) = %zu  (같은 타입이다)\n",
           sizeof(x), sizeof(struct x));
    return 0;
}
