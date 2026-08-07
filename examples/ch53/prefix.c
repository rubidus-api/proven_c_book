/* 「모듈 하나 = 외부 심볼 하나」 — 접두어 규약과 함수 포인터 표.
   C 에는 사용자가 만드는 이름 공간이 없으니, 내보내는 이름을 줄이는 것이
   가장 확실한 방어다. */
#include <stdio.h>
#include <string.h>

/* ── 안쪽 구현은 전부 내부 연결(static) — 바깥 마당에 나가지 않는다 ── */
static int  buf_len;
static char buf[64];

static void impl_reset(void)                 { buf_len = 0; buf[0] = '\0'; }
static bool impl_push(const char *s)
{
    size_t n = strlen(s);
    if ((size_t)buf_len + n + 1 > sizeof buf) return false;   /* 잘림은 실패다(40장) */
    memcpy(buf + buf_len, s, n + 1);
    buf_len += (int)n;
    return true;
}
static const char *impl_text(void)           { return buf; }

/* ── 밖에 내보내는 것은 이것 하나 ──
   표(vtable) 하나에 접두어를 붙여 두면, 이 번역 단위가 링커에 내미는
   이름은 `textbuf` 단 하나다. 나머지 철자는 다른 파일과 부딪힐 수 없다. */
struct textbuf_api {
    void        (*reset)(void);
    bool        (*push)(const char *);
    const char *(*text)(void);
};

const struct textbuf_api textbuf = {
    .reset = impl_reset,
    .push  = impl_push,
    .text  = impl_text,
};

/* ── 쓰는 쪽 ── */
int main(void)
{
    textbuf.reset();
    puts("[모듈 하나가 내미는 외부 이름은 `textbuf` 하나뿐이다]");
    printf("  push(\"hello \") -> %s\n", textbuf.push("hello ") ? "ok" : "실패");
    printf("  push(\"world\")  -> %s\n", textbuf.push("world")  ? "ok" : "실패");
    printf("  text()         -> \"%s\"\n", textbuf.text());

    char big[80];
    memset(big, 'A', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    printf("  긴 문자열 push -> %s (잘림을 성공으로 치지 않는다)\n",
           textbuf.push(big) ? "ok" : "실패");

    puts("\n[접두어 규약 — 큰 프로젝트의 관행]");
    puts("  sqlite3_ / curl_ / SSL_ / g_ / SDL_ … 타입·매크로에도 같은 접두어를");
    puts("  붙인다. 접두어는 C 에 없는 이름 공간을 사람이 손으로 대신하는 것이다.");
    return 0;
}
