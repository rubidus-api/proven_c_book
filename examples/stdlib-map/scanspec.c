#include <stdio.h>

int main(void) {
    int a = 0, b = 0, k;
    double d = 0.0;
    char word[8] = {0};
    char rest[16] = {0};

    /* 서식의 공백 하나가 "공백 몇 개든" 을 뜻한다 */
    k = sscanf("  12   34", "%d %d", &a, &b);
    printf("ints    : k=%d a=%d b=%d\n", k, a, b);

    /* %s 는 공백에서 멈춘다. 폭을 주어 버퍼를 지킨다 */
    k = sscanf("hello world", "%7s", word);
    printf("bounded : k=%d word=[%s]\n", k, word);

    /* 서식 안의 보통 글자는 입력과 그대로 맞아야 한다 */
    k = sscanf("x=5", "x=%d", &a);
    printf("literal : k=%d a=%d\n", k, a);

    /* 맞지 않으면 매칭 실패 — 0 을 돌려주고 인자는 건드리지 않는다 */
    k = sscanf("y=5", "x=%d", &a);
    printf("mismatch: k=%d (a stays %d)\n", k, a);

    /* 변환 실패도 0 이다 — 숫자가 아니면 아무것도 읽지 않는다 */
    k = sscanf("abc", "%d", &b);
    printf("nonnum  : k=%d (b stays %d)\n", k, b);

    /* 입력이 비면 EOF(음수)다 — 0 과 구별해야 한다 */
    k = sscanf("", "%d", &b);
    printf("empty   : k=%d\n", k);

    /* 부분 성공: 앞은 읽히고 뒤에서 멈춘다 */
    k = sscanf("7 oops", "%d %lf", &a, &d);
    printf("partial : k=%d a=%d\n", k, a);

    /* 집합 지정자: 쉼표가 아닌 글자를 모은다 */
    k = sscanf("name,42", "%15[^,],%d", rest, &b);
    printf("set     : k=%d rest=[%s] b=%d\n", k, rest, b);

    /* 실수와 길이 수식어: double 은 %lf 다 */
    k = sscanf("3.5", "%lf", &d);
    printf("double  : k=%d d=%.2f\n", k, d);
    return 0;
}
