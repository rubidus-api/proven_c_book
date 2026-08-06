#include <stdio.h>
#include <stdlib.h>

/* 아래는 전부 올바른 C 코드다. 그런데 C++ 컴파일러에 그대로 넣으면
   상당수가 컴파일되지 않는다. 이 예제는 C 로만 빌드된다. */

struct point { int x, y; };

/* ① C++ 의 예약어가 C 에서는 평범한 이름이다 */
static int class_of(int n) { return n / 10; }

int main(void)
{
    /* ② void* 에서 다른 포인터로의 암묵 변환 — C 는 허용, C++ 는 거부 */
    int *buf = malloc(4 * sizeof *buf);
    if (!buf) return 1;
    for (int i = 0; i < 4; i++) buf[i] = i * i;

    /* ③ 문자 상수의 타입: C 에서는 int 다 */
    printf("sizeof('a') = %zu  (C++ 에서는 1 이 나온다)\n", sizeof('a'));

    /* ④ 순서를 바꾼 지정 초기화 — C99 는 허용 */
    struct point p = { .y = 2, .x = 1 };
    printf("point = (%d, %d)\n", p.x, p.y);

    /* ⑤ 구조체 태그는 별도 이름 공간이라 struct 를 붙여야 한다 */
    struct point q = p;
    printf("copy  = (%d, %d)\n", q.x, q.y);

    printf("class_of(37) = %d\n", class_of(37));
    printf("buf = %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);

    free(buf);
    return 0;
}
