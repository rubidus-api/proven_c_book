/* for 의 첫 칸(clause-1)에서 무엇을 선언할 수 있는가 — 실측판.
   C23 로 빌드한다. 주석에 적은 「C99·C11 에서는」 은 -std=c11 -pedantic-errors
   로 확인한 결과다. */
#include <stdio.h>

/* ① 한 번의 선언이므로 기본 타입은 하나다. 파생 타입은 함께 적을 수 있다. */
static void one_base_type(void)
{
    int a[3] = { 10, 20, 30 };

    /* int 하나로 값·포인터·개수를 한꺼번에 만든다 */
    for (int *p = a, *end = a + 3, n = 0; p != end; p++, n++)
        printf("  a[%d] = %d\n", n, *p);

    /* for (int i = 0; double x = 0.0;) 는 문법 오류다 —
       "expected expression before 'double'" (gcc·clang 모두) */
}

/* ② 이름의 수명은 루프 하나다 — 세 칸과 몸통까지, 그리고 거기서 끝난다. */
static void scope_of_the_name(void)
{
    int i = 100;                        /* 바깥의 i */

    for (int i = 0; i < 2; i++)         /* 안쪽 i 가 바깥 i 를 가린다 */
        printf("  inside the loop i = %d\n", i);

    printf("  after the loop i = %d (the outer one was never touched)\n", i);
}

/* ②-b 수명 — 첫 칸의 변수는 루프 하나에 대해 *객체 하나*다.
      몸통 안에서 선언한 변수는 그렇지 않다: 매 바퀴 새로 태어나고 매 바퀴 죽는다. */
static void lifetime_of_the_two(void)
{
    int last_body = -1;

    for (int i = 0; i < 3; i++) {
        int body = 0;                   /* 매 바퀴 새 객체 — 그래서 늘 0 부터 */
        body++;
        last_body = body;
        printf("  turn %d: counter i = %d (one object, kept), body = %d (reborn)\n",
               i, i, body);
    }
    printf("  the body variable never grew past %d - it died at every closing brace\n",
           last_body);
}

/* ②-c 같은 객체라는 것은 주소로 확인된다 — 세 바퀴 모두 같은 주소다. */
static void one_object_one_address(void)
{
    const void *first = nullptr;

    for (int i = 0; i < 3; i++) {
        if (i == 0)
            first = (const void *)&i;
        printf("  turn %d: &i %s\n", i,
               (const void *)&i == first ? "is the same address as turn 0"
                                         : "CHANGED (would mean a new object)");
    }
    /* 루프를 벗어나면 그 객체의 수명이 끝난다. 위에서 받아 둔 주소를
       이제 와서 읽는 것은 정의되지 않은 동작이라 여기서는 쓰지 않는다.
       이름 자체도 사라진다 --- 여기서 i 를 적으면 컴파일 오류다. */
}

/* ③ C23 의 auto — 첫 칸에서 타입을 초기값으로부터 추론한다. */
static void c23_auto(void)
{
    for (auto i = 0; i < 3; i++)        /* i 는 int 로 추론된다 */
        printf("  auto i = %d\n", i);
}

/* ④ C23 은 저장 클래스 제약을 없앴다 — 그래서 static 도 적을 수 있다.
      적을 수 있다는 것과 적어야 한다는 것은 다르다. 이 함수가 그 이유다. */
static void static_counter(void)
{
    for (static int calls = 0; calls < 2; calls++)
        printf("  static clause-1: this line ran (calls = %d)\n", calls);
}

int main(void)
{
    printf("[one declaration means one base type]\n");
    one_base_type();

    printf("\n[the name lives in the loop and nowhere else]\n");
    scope_of_the_name();

    printf("\n[the counter lives for the whole loop, a body variable does not]\n");
    lifetime_of_the_two();

    printf("\n[one object means one address]\n");
    one_object_one_address();

    printf("\n[C23: auto infers the type from the initializer]\n");
    c23_auto();

    printf("\n[C23: a static object in clause-1 is initialized once, ever]\n");
    printf("  first call:\n");
    static_counter();
    printf("  second call:\n");
    static_counter();
    printf("  third call:\n");
    static_counter();
    printf("  the second and third calls print nothing - the counter kept its\n");
    printf("  value from the first call, so the loop never ran again.\n");
    return 0;
}
