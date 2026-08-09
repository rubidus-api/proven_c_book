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

    puts("[do { } while (0) — 매크로가 한 문장이 된다]");
    if (x < y) SWAP(x, y); else puts("  (여기 오지 않는다)");
    printf("  교환 뒤: x=%d y=%d   ← if/else 사이에서도 깨지지 않는다\n", x, y);

    LOG("값 %d 와 %d", x, y);

    puts("\n[for 의 몸통으로 써도 중괄호 없이 안전하다]");
    for (int i = 0; i < 2; i++)
        LOG("반복 %d", i);

    puts("\n[goto 로 정리 지점 모으기]");
    printf("  둘 다 성공: %s\n", build(16, false) ? "ok" : "실패");
    printf("  둘째에서 실패: %s  ← 첫째만 되돌리고 빠져나왔다\n",
           build(16, true) ? "ok" : "실패");
    return 0;
}
