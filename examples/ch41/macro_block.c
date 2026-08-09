/* 매크로를 '한 문장'으로 만드는 do { } while (0), 그리고 goto 로 정리하기. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 중괄호만 쓰면 뒤에 세미콜론이 붙는 순간 문장이 둘이 된다.
   그래서 아래처럼 if/else 사이에 놓으면 컴파일 오류가 난다.

       #define SWAP_BAD(a, b) { int t = (a); (a) = (b); (b) = t; }
       if (x < y) SWAP_BAD(x, y); else puts("...");
           → error: 'else' without a previous 'if'

   do { } while (0) 은 '중괄호 블록이면서 동시에 한 문장'이 되게 한다. */
#define SWAP(a, b)  do { int t_ = (a); (a) = (b); (b) = t_; } while (0)

#define LOG(fmt, ...)  do {                       \
        printf("[log] " fmt "\n", __VA_ARGS__);   \
    } while (0)

/* goto 로 정리 지점을 한 곳에 모으는 무늬 — 리눅스 커널이 쓰는 그 형태다.
   중간에 실패해도 '거기까지 얻은 것만' 되돌린다. */
static bool build(size_t n, bool fail_at_second)
{
    char *a = nullptr, *b = nullptr;
    bool ok = false;

    a = malloc(n);
    if (!a) goto out;                 /* 아직 되돌릴 것이 없다 */
    memset(a, 'a', n);

    b = fail_at_second ? nullptr : malloc(n);
    if (!b) goto free_a;              /* a 만 되돌린다 */
    memset(b, 'b', n);

    ok = true;

    free(b);
free_a:
    free(a);
out:
    return ok;
}

int main(void)
{
    int x = 1, y = 2;

    puts("[do { } while (0) - a macro becomes one statement]");
    if (x < y) SWAP(x, y); else puts("  (we never get here)");
    printf("  after the swap: x=%d y=%d   <- it does not break between if and else\n", x, y);

    LOG("values %d and %d", x, y);

    puts("\n[safe as the body of a for, even without braces]");
    for (int i = 0; i < 2; i++)
        LOG("round %d", i);

    puts("\n[gathering cleanup with goto]");
    printf("  both succeeded: %s\n", build(16, false) ? "ok" : "failed");
    printf("  second one failed: %s  <- only the first was undone before leaving\n",
           build(16, true) ? "ok" : "failed");
    return 0;
}
