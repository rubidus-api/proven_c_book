/* 스코프는 '선언자가 끝난 지점'부터 시작한다 --- 등호 앞이 아니라. */
#include <stdio.h>

typedef int meter;          /* 파일 스코프의 타입 이름 */

int main(void)
{
    /* i 의 스코프는 = 를 만나기 *전에* 이미 시작했다.
       그래서 sizeof(i) 는 방금 선언한 그 i 를 가리킨다(값은 읽지 않는다). */
    int i = sizeof(i);
    printf("int i = sizeof(i);  -> %d\n", i);

    /* 자기 자신의 주소를 담는 포인터도 같은 이치로 성립한다 */
    void *self = &self;
    printf("void *self = &self; -> self == &self is %s\n",
           self == (void *)&self ? "true" : "false");

    {
        /* 이 자리에서 meter 는 타입 이름이다. 그러나 아래 한 줄이 지나면
           이 블록에서 meter 는 *변수*다 --- 선언자가 끝난 지점부터. */
        meter meter = 42;
        printf("meter meter = 42;   -> %d  (the type name is now shadowed)\n", meter);
    }

    /* 블록을 벗어나면 meter 는 다시 타입 이름이다 */
    meter distance = 7;
    printf("outside the block, meter is a type again -> %d\n", distance);

    /* for 의 첫 칸에 선언한 이름은 루프가 끝나면 사라진다 */
    for (int n = 0; n < 3; n++) { /* n 은 여기서만 */ }
    /* 여기서 n 은 이름이 아니다 --- 같은 이름을 다시 써도 서로 무관하다 */
    for (int n = 10; n > 8; n--) { /* 앞의 n 과 아무 관계가 없다 */ }
    puts("\ntwo loops used the name n; neither knows the other");
    return 0;
}
