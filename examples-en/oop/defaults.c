/* Empty slots in the table --- the two conventions side by side.
   Convention 1: a slot with nothing to do is left NULL, and the caller checks each slot.
   Convention 2: the default behaviour is a function, a "bundle of defaults" macro fills
                 every slot first, and each kind overrides only the slots that differ
                 (N. Brown, LWN 2011). The caller just calls.
   Two kinds of stream: the "file" collects what is written and sends it out on flush; the
   "memory" stream stores at once, so it has nothing to flush. Suppose close is a slot
   that was *added to the table later*. */
#include <stdio.h>

struct stream;
struct stream_ops {
    void (*write)(struct stream *s, const char *text);
    void (*flush)(struct stream *s);
    void (*close)(struct stream *s);    /* added later */
};
struct stream { const struct stream_ops *ops; const char *name; int pending; };

static void file_write(struct stream *s, const char *text)
{
    (void)text;
    s->pending++;                       /* only collects */
}
static void file_flush(struct stream *s)
{
    printf("  %s: flushed %d pending writes\n", s->name, s->pending);
    s->pending = 0;
}
static void memory_write(struct stream *s, const char *text)
{
    printf("  %s: stored \"%s\"\n", s->name, text);
}

/* ---- convention 1: empty slots ---- */
static const struct stream_ops file_ops_1   = { .write = file_write, .flush = file_flush };
static const struct stream_ops memory_ops_1 = { .write = memory_write };
/* neither table knows close --- written before the slot existed, so it is NULL */

static void finish_1(struct stream *s)
{
    if (s->ops->flush)                  /* every caller, every slot: this check */
        s->ops->flush(s);
    if (s->ops->close)
        s->ops->close(s);
}

/* ---- convention 2: a bundle of defaults ---- */
static void stream_flush_nothing(struct stream *s) { (void)s; }
static void stream_close_default(struct stream *s)
{
    printf("  %s: closed (default close)\n", s->name);
}
#define STREAM_OPS_DEFAULTS \
    .flush = stream_flush_nothing, .close = stream_close_default

/* Initialising the same slot twice is allowed and the later one wins (C23 §6.7.11p20).
   But the compiler cannot tell this from a slot written twice by mistake, so it warns
   (GCC's -Woverride-init under -Wextra, Clang's -Winitializer-overrides). The warning is
   turned off only for these few lines that declare the tables. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
static const struct stream_ops file_ops_2 = {
    STREAM_OPS_DEFAULTS,
    .write = file_write,
    .flush = file_flush,                /* overrides the default */
};
static const struct stream_ops memory_ops_2 = {
    STREAM_OPS_DEFAULTS,
    .write = memory_write,              /* flush and close stay the defaults */
};
#pragma GCC diagnostic pop

static void finish_2(struct stream *s)
{
    s->ops->flush(s);                   /* no empty slots, so no checks */
    s->ops->close(s);
}

int main(void)
{
    puts("convention 1: empty slots, the caller checks each one");
    struct stream f1 = { .ops = &file_ops_1, .name = "file" };
    struct stream m1 = { .ops = &memory_ops_1, .name = "memory" };
    f1.ops->write(&f1, "a");
    f1.ops->write(&f1, "b");
    m1.ops->write(&m1, "c");
    finish_1(&f1);
    finish_1(&m1);
    printf("  memory flush slot: %s\n", memory_ops_1.flush ? "filled"
           : "NULL --- a caller that forgets the check calls a null pointer");
    printf("  close slot in both tables: %s\n",
           file_ops_1.close || memory_ops_1.close ? "filled" : "NULL --- nobody closes");

    puts("convention 2: defaults filled in, the caller just calls");
    struct stream f2 = { .ops = &file_ops_2, .name = "file" };
    struct stream m2 = { .ops = &memory_ops_2, .name = "memory" };
    f2.ops->write(&f2, "a");
    f2.ops->write(&f2, "b");
    m2.ops->write(&m2, "c");
    finish_2(&f2);
    finish_2(&m2);
    return 0;
}
