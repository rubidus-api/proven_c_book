#include <proven.h>

int main(void)
{
    const char *name = "world";
    int         count = 3;
    double      ratio = 0.75;
    bool        ready = true;
    char        grade = 'A';

    /* {} 는 자리표시자일 뿐 타입이 없다. 타입은 인자에서 온다 */
    proven_println("Hello, {}! You have {} messages.",
                   PROVEN_ARG(name), PROVEN_ARG(count));

    /* 어떤 타입이든 같은 자리표시자로 간다 */
    proven_println("ratio={} ready={} grade={}",
                   PROVEN_ARG(ratio), PROVEN_ARG(ready), PROVEN_ARG(grade));

    /* 서식 지정은 콜론 뒤에 붙인다 — 폭, 정렬, 자릿수 */
    proven_println("|{:>8}|{:<8}|{:.3}|",
                   PROVEN_ARG(name), PROVEN_ARG(name), PROVEN_ARG(ratio));
    return 0;
}
