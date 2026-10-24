/* C23 의 속성 — 타입이 담지 못하는 「뜻」을 컴파일러에게 적어 두는 법 */
#include <stdio.h>
#include <stdlib.h>

/* ① nodiscard — 돌려준 값을 버리면 안 되는 함수.
   실패를 값으로 알리는 함수에 붙이면, 확인을 잊은 코드가 경고가 된다. */
[[nodiscard]] static int checked_add(int a, int b, int *out)
{
    if (b > 0 && a > 2147483647 - b) return 0;   /* 넘침: 실패 */
    *out = a + b;
    return 1;
}

/* ② maybe_unused — 빌드 구성에 따라 안 쓰일 수도 있는 것에 미리 붙인다 */
static void log_line([[maybe_unused]] const char *tag, const char *msg)
{
#ifdef VERBOSE
    printf("  [%s] %s\n", tag, msg);
#else
    printf("  %s\n", msg);
#endif
}

/* ③ noreturn — 돌아오지 않는 함수. 부르는 쪽의 뒷줄은 죽은 코드가 된다 */
[[noreturn]] static void die(const char *why)
{
    printf("  fatal: %s\n", why);
    exit(EXIT_FAILURE);
}

static const char *classify(int n)
{
    switch (n) {
    case 0:
        printf("  zero, and it keeps going\n");
        [[fallthrough]];          /* ④ 일부러 흘린다는 표시 */
    case 1:
        return "small";
    default:
        return "large";
    }
}

int main(void)
{
    int sum = 0;

    if (checked_add(2000000000, 2000000000, &sum))
        printf("  sum: %d\n", sum);
    else
        printf("  overflow refused\n");

    if (checked_add(2, 3, &sum))
        printf("  sum: %d\n", sum);

    log_line("main", "attributes carry intent, not types");
    printf("  classify(0) = %s\n", classify(0));
    printf("  classify(9) = %s\n", classify(9));

    if (sum != 5) die("checked_add lost a value");
    return 0;
}
