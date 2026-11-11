/* The print backend: instead of hardware, print each command received, one per line.
   Reads need an answer, so there is one rule --- an address in the reply table
   (trace_replies.h, different for each example) returns the table's values in turn (the
   last one repeats); any other address returns the last value written to it, and one
   never written returns TRACE_DEFAULT. That table is all the device imitation there is.
   Bytes at the address named TRACE_TEXT are also printed as characters. When accesses
   of the same shape follow at a constant stride, two lines are printed and the rest
   collapse into one (so that a bus scan does not cover the page). */
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

/* how many values have been handed out */
static unsigned used[sizeof replies / sizeof *replies];

/* put 1 in *scripted if the value came from the table, 0 if it is the last value
   written */
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

/* Collapsing: same read/write, value and width as the line before, at the same stride,
   is one run. */
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

/* A report is not a command, so it gets a leading # to keep it apart. Only at the start
   of a line. */
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
