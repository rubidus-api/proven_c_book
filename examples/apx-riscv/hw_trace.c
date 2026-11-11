/* 출력 백엔드: 하드웨어 대신 받은 명령을 한 줄씩 찍는다.
   읽기에는 대답이 있어야 하므로 규칙 하나를 둔다 --- 대답 표(trace_replies.h, 예제마다
   다르다)에 있는 주소는 표의 값을 차례로(마지막 값은 되풀이해) 돌려주고, 그 밖의
   주소는 마지막으로 쓴 값을, 한 번도 쓰지 않은 주소는 TRACE_DEFAULT 를 돌려준다.
   이 표가 장치 흉내의 전부다. TRACE_TEXT 로 정한 주소에 오가는 바이트는 글자로도 찍는다.
   같은 꼴의 접근이 일정한 간격으로 이어지면 두 줄만 찍고 나머지는 한 줄로 줄인다
   (버스 훑기가 지면을 덮지 않게). */
#include <stdio.h>
#include <stdlib.h>
#include "hw.h"
#include "trace_replies.h"
#ifndef TRACE_DEFAULT
#define TRACE_DEFAULT 0u
#endif

static struct { uintptr_t addr; uint32_t value; } last[256];
static unsigned nlast;

static void remember(uintptr_t addr, uint32_t v)
{
    for (unsigned i = 0; i < nlast; i++)
        if (last[i].addr == addr) { last[i].value = v; return; }
    if (nlast < sizeof last / sizeof *last) {
        last[nlast].addr = addr;
        last[nlast].value = v;
        nlast++;
    }
}

static unsigned used[sizeof replies / sizeof *replies];   /* 몇 번째 값까지 냈나 */

/* 표에서 나온 값이면 1 을, 마지막으로 쓴 값이면 0 을 *scripted 에 둔다 */
static uint32_t answer(uintptr_t addr, int *scripted)
{
    *scripted = 1;
    for (unsigned i = 0; i < sizeof replies / sizeof *replies; i++)
        if (replies[i].addr == addr) {
            unsigned k = used[i] < replies[i].n - 1 ? used[i]++ : replies[i].n - 1;
            return replies[i].values[k];
        }
    *scripted = 0;
    for (unsigned i = 0; i < nlast; i++)
        if (last[i].addr == addr) return last[i].value;
    return TRACE_DEFAULT;
}

/* 줄여 찍기: 앞 줄과 읽기/쓰기 · 값 · 폭이 같고 주소 간격이 같으면 한 무리다. */
static struct { char rw; uintptr_t addr; uint32_t v; int width; uintptr_t step; unsigned n; } run;

static void flush_run(void)
{
    if (run.n > 2 && run.step == 0)
        printf("   ... the same %u more times\n", run.n - 2);
    else if (run.n > 2)
        printf("   ... %u more like this, every 0x%jx, up to 0x%08jx\n",
               run.n - 2, (uintmax_t)run.step, (uintmax_t)run.addr);
    run.n = 0;
}

static void say(char rw, uintptr_t addr, uint32_t v, int width, int show_char)
{
    static int armed;
    if (!armed) { atexit(flush_run); armed = 1; }
    if (run.n && rw == run.rw && v == run.v && width == run.width && addr >= run.addr
        && (run.n == 1 || addr - run.addr == run.step)) {
        run.step = addr - run.addr;
        run.addr = addr;
        if (++run.n > 2) return;
    } else {
        flush_run();
        run.rw = rw; run.addr = addr; run.v = v; run.width = width; run.n = 1;
    }
    printf("%c  0x%08jx %s 0x%0*x", rw, (uintmax_t)addr, rw == 'R' ? "->" : "<-",
           width * 2, (unsigned)v);
#ifdef TRACE_TEXT
    if (show_char && addr == TRACE_TEXT && v >= 0x20 && v < 0x7f) printf(" '%c'", (int)v);
#else
    (void)show_char;
#endif
    putchar('\n');
}

uint8_t hw_read8(uintptr_t addr)
{ int s; uint8_t v = (uint8_t)answer(addr, &s); say('R', addr, v, 1, !s); return v; }
uint32_t hw_read32(uintptr_t addr)
{ int s; uint32_t v = answer(addr, &s); say('R', addr, v, 4, 0); return v; }
void hw_write8(uintptr_t addr, uint8_t v)
{ remember(addr, v); say('W', addr, v, 1, 1); }
void hw_write32(uintptr_t addr, uint32_t v)
{ remember(addr, v); say('W', addr, v, 4, 0); }

/* 결과 알림은 명령이 아니므로 앞에 # 를 붙여 섞이지 않게 한다. 줄의 처음에만 붙인다. */
void hw_log(const char *s)
{
    static int bol = 1;
    flush_run();
    for (; *s; s++) {
        if (bol) fputs("#  ", stdout);
        putchar(*s);
        bol = *s == '\n';
    }
}
