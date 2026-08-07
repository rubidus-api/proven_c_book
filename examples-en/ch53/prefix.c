/* "One module, one external symbol" — the prefix convention and a table of
   function pointers. C has no user-made name spaces, so cutting down the
   names you export is the surest defence. */
#include <stdio.h>
#include <string.h>

/* -- the implementation is all internal linkage: it never leaves this file -- */
static int  buf_len;
static char buf[64];

static void impl_reset(void)                 { buf_len = 0; buf[0] = '\0'; }
static bool impl_push(const char *s)
{
    size_t n = strlen(s);
    if ((size_t)buf_len + n + 1 > sizeof buf) return false;   /* truncation is failure (ch.40) */
    memcpy(buf + buf_len, s, n + 1);
    buf_len += (int)n;
    return true;
}
static const char *impl_text(void)           { return buf; }

/* -- this is the only thing exported --
   Put the prefix on one table and the only name this translation unit hands
   to the linker is `textbuf`. No other spelling here can collide. */
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

/* -- the calling side -- */
int main(void)
{
    textbuf.reset();
    puts("[the module hands out exactly one external name: textbuf]");
    printf("  push(\"hello \") -> %s\n", textbuf.push("hello ") ? "ok" : "failed");
    printf("  push(\"world\")  -> %s\n", textbuf.push("world")  ? "ok" : "failed");
    printf("  text()         -> \"%s\"\n", textbuf.text());

    char big[80];
    memset(big, 'A', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    printf("  pushing a long string -> %s (truncation is not success)\n",
           textbuf.push(big) ? "ok" : "failed");

    puts("\n[the prefix convention — what large projects do]");
    puts("  sqlite3_ / curl_ / SSL_ / g_ / SDL_ ... the same prefix goes on");
    puts("  types and macros too. A prefix is a name space built by hand.");
    return 0;
}
