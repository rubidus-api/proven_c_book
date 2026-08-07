/* 예외 흉내 — libjpeg 계열이 쓰는 오류 처리 무늬, 그리고 그 대가. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── libjpeg 의 error manager 를 줄여 옮긴 모양 ──────────────────
   구조체 안에 jmp_buf 를 넣어 두고, 깊은 곳에서 오류가 나면 그리로 뛴다.
   실제 libjpeg 의 setup_error_handler / png_jmpbuf 가 이 무늬다. */
typedef struct {
    jmp_buf env;
    char    message[64];
    int     code;
} error_ctx;

static error_ctx *current;          /* 지금 활성인 문맥(전역 하나) */

[[noreturn]] static void throw_error(int code, const char *msg)
{
    snprintf(current->message, sizeof current->message, "%s", msg);
    current->code = code;
    longjmp(current->env, 1);       /* 호출자의 setjmp 자리로 */
}

/* 깊은 곳의 함수들 — 오류를 값으로 돌려주지 않는다 */
static int parse_header(const char *s)
{
    if (strncmp(s, "IMG", 3) != 0) throw_error(2, "매직이 맞지 않는다");
    return 3;
}

static int parse_size(const char *s)
{
    int n = atoi(s);
    if (n <= 0)      throw_error(3, "크기가 0 이하다");
    if (n > 1000000) throw_error(4, "크기가 너무 크다");
    return n;
}

/* ── 흐름을 감싸는 쪽 ──────────────────────────────────────────── */
static int load_image(const char *text, char *err, size_t errcap)
{
    error_ctx ctx = { .code = 0 };
    error_ctx *saved = current;
    current = &ctx;

    /* 여기서 자원을 잡는다 — longjmp 로 돌아오면 *직접* 풀어야 한다 */
    void *buffer = malloc(1024);
    if (!buffer) { current = saved; return -1; }

    volatile int result;            /* longjmp 를 건너 살아남는다 */
    if (setjmp(ctx.env) == 0) {
        int off = parse_header(text);
        int n   = parse_size(text + off);
        printf("  성공: 크기 %d\n", n);
        result = n;
    } else {
        snprintf(err, errcap, "%s (코드 %d)", ctx.message, ctx.code);
        result = -1;
    }

    free(buffer);                   /* 두 경로 모두 여기를 지난다 — 설계의 핵심 */
    current = saved;
    return result;
}

int main(void)
{
    const char *cases[] = { "IMG640", "BMP640", "IMG0", "IMG9999999" };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        char err[96] = "";
        printf("입력 \"%s\":\n", cases[i]);
        int rc = load_image(cases[i], err, sizeof err);
        if (rc < 0) printf("  실패: %s\n", err);
    }

    puts("\n이 무늬가 성립하는 조건 셋:");
    puts("  ① 자원을 푸는 자리가 setjmp 를 부른 함수 안에 *한 군데* 모여 있다");
    puts("  ② longjmp 를 건너는 지역 변수에 volatile 이 붙어 있다");
    puts("  ③ setjmp 를 부른 함수가 아직 살아 있는 동안에만 longjmp 한다");
    return 0;
}
