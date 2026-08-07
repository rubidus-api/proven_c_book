/* imitating exceptions — the error pattern libjpeg's family uses, and its price. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── a shrunken copy of libjpeg's error manager ────────────────────
   A jmp_buf sits inside the struct, and a failure deep down jumps to it.
   libjpeg's setup_error_handler and png_jmpbuf are exactly this shape. */
typedef struct {
    jmp_buf env;
    char    message[64];
    int     code;
} error_ctx;

static error_ctx *current;          /* the context in force (one global) */

[[noreturn]] static void throw_error(int code, const char *msg)
{
    snprintf(current->message, sizeof current->message, "%s", msg);
    current->code = code;
    longjmp(current->env, 1);       /* to the caller's setjmp */
}

/* the deep functions — they do not return errors as values */
static int parse_header(const char *s)
{
    if (strncmp(s, "IMG", 3) != 0) throw_error(2, "magic does not match");
    return 3;
}

static int parse_size(const char *s)
{
    int n = atoi(s);
    if (n <= 0)      throw_error(3, "size is zero or less");
    if (n > 1000000) throw_error(4, "size is too large");
    return n;
}

/* ── the side that wraps the flow ──────────────────────────────── */
static int load_image(const char *text, char *err, size_t errcap)
{
    error_ctx ctx = { .code = 0 };
    error_ctx *saved = current;
    current = &ctx;

    /* resources are taken here — coming back by longjmp, we must free them *ourselves* */
    void *buffer = malloc(1024);
    if (!buffer) { current = saved; return -1; }

    volatile int result;            /* survives across the longjmp */
    if (setjmp(ctx.env) == 0) {
        int off = parse_header(text);
        int n   = parse_size(text + off);
        printf("  success: size %d\n", n);
        result = n;
    } else {
        snprintf(err, errcap, "%s (code %d)", ctx.message, ctx.code);
        result = -1;
    }

    free(buffer);                   /* both paths pass here — the heart of the design */
    current = saved;
    return result;
}

int main(void)
{
    const char *cases[] = { "IMG640", "BMP640", "IMG0", "IMG9999999" };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        char err[96] = "";
        printf("input \"%s\":\n", cases[i]);
        int rc = load_image(cases[i], err, sizeof err);
        if (rc < 0) printf("  failure: %s\n", err);
    }

    puts("\nThree conditions this pattern needs:");
    puts("  (1) freeing happens in *one place* inside the function that called setjmp");
    puts("  (2) locals that cross the longjmp carry volatile");
    puts("  (3) longjmp happens only while that function is still alive");
    return 0;
}
