/* 실무에서 실제로 쓰인 난해한 선언들 — 그리고 절차대로 읽으면 풀린다는 것. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/* ── ① 표준의 signal — C 표준 §7.14.1.1 에 그대로 있는 선언 ────────
      void (*signal(int sig, void (*func)(int)))(int);
   같은 것을 typedef 로 층을 나누면 이렇게 된다. */
typedef void handler_t(int);          /* 함수 타입 자체에 이름을 준다 */
static void on_int(int sig) { (void)sig; }

/* ── ② X11 의 XSetErrorHandler 를 흉내 낸 모양 ───────────────────
      int (*XSetErrorHandler(int (*handler)(Display *, XErrorEvent *)))();
   Display·XErrorEvent 자리에 흉내용 타입을 넣어 그대로 옮겼다. */
typedef struct { int dummy; } Display;
typedef struct { int code; } XErrorEvent;

static int my_error_handler(Display *d, XErrorEvent *e)
{ (void)d; printf("  error handler: code=%d\n", e->code); return 0; }

/* 날것 그대로: "핸들러를 받아, 이전 핸들러를 돌려주는 함수" */
static int (*set_error_handler(int (*handler)(Display *, XErrorEvent *)))
           (Display *, XErrorEvent *)
{
    static int (*current)(Display *, XErrorEvent *);
    int (*prev)(Display *, XErrorEvent *) = current;
    current = handler;
    return prev;
}

/* 같은 것을 typedef 로 — 한 줄이 세 줄이 되고, 세 줄이 다 읽힌다 */
typedef int error_handler_t(Display *, XErrorEvent *);
static error_handler_t *set_error_handler2(error_handler_t *handler)
{ return set_error_handler(handler); }

/* ── ③ 함수를 가리키는 포인터의 배열 — 명령 표(dispatch table) ──── */
static int cmd_add(int a, int b) { return a + b; }
static int cmd_mul(int a, int b) { return a * b; }

/* int (*table[2])(int, int) — "int 둘을 받아 int 를 돌려주는 함수를
   가리키는 포인터"의 배열. 절차대로 읽으면 이렇게 나온다. */
static int (*table[2])(int, int) = { cmd_add, cmd_mul };

/* ── ④ 배열을 가리키는 포인터를 돌려주는 함수 ──────────────────── */
static int (*rows(void))[4]            /* "int[4] 를 가리키는 포인터를 돌려주는 함수" */
{
    static int grid[3][4] = { {1,2,3,4}, {5,6,7,8}, {9,10,11,12} };
    return grid;                        /* int (*)[4] 로 무너진다 */
}

int main(void)
{
    /* ① signal 은 *이전 핸들러*를 돌려준다 — 그래서 반환 타입이 함수 포인터다 */
    handler_t *old = signal(SIGINT, on_int);
    printf("signal returns the previous handler: %s\n",
           old == SIG_ERR ? "SIG_ERR" : "there was one");
    (void)signal(SIGINT, old == SIG_ERR ? SIG_DFL : old);

    /* ② 같은 꼴 — 설치하고, 이전 것을 돌려받는다 */
    puts("\ninstalling an X11-style error handler:");
    int (*prev)(Display *, XErrorEvent *) = set_error_handler(my_error_handler);
    printf("  previous handler: %s\n", prev ? "present" : "none (first install)");
    Display d = { 0 };
    XErrorEvent e = { .code = 42 };
    error_handler_t *prev2 = set_error_handler2(my_error_handler);  /* typedef 판 */
    printf("  the typedef version does the same: previous handler %s\n",
           prev2 == my_error_handler ? "identical" : "different");
    my_error_handler(&d, &e);

    /* ③ 명령 표 */
    printf("\ncommand table: add(3,4)=%d, mul(3,4)=%d\n", table[0](3, 4), table[1](3, 4));

    /* ④ 배열을 가리키는 포인터 */
    int (*g)[4] = rows();
    printf("pointer to an array: g[2][1] = %d (one step is %zu bytes)\n",
           g[2][1], sizeof *g);
    return 0;
}
