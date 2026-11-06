/* Building tables at run time, in five steps: register, fill, verify, seal, use.
   The kinds arrive as data (as a plugin list would), not as `static const` tables.
   Filling goes defaults -> parent -> own slots, so a kind names only what differs.
   Sealing makes the page that holds the tables read-only (POSIX mprotect).
   Run with --write-after-seal to try to change a slot afterwards. */
#define _DEFAULT_SOURCE        /* MAP_ANONYMOUS: Linux and the BSDs, POSIX 2024 */
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct stream;
typedef int (*slot)(struct stream *);
enum { WRITE, FLUSH, CLOSE, SLOTS };
static const char *const slot_names[SLOTS] = { "write", "flush", "close" };
struct stream_ops { slot s[SLOTS]; };
struct stream { const struct stream_ops *ops; const char *kind; };

/* behaviours --- each just says who it is */
#define SAY(fn) static int fn(struct stream *st) { (void)st; return 0; }
SAY(stream_flush_nothing) SAY(stream_close) SAY(file_write) SAY(file_flush)
SAY(gzip_write) SAY(gzip_close) SAY(memory_write)
static const struct { slot fn; const char *name; } fn_names[] = {
    { stream_flush_nothing, "stream_flush_nothing" }, { stream_close, "stream_close" },
    { file_write, "file_write" }, { file_flush, "file_flush" },
    { gzip_write, "gzip_write" }, { gzip_close, "gzip_close" },
    { memory_write, "memory_write" },
};
static const char *name_of(slot f)
{
    for (size_t i = 0; i < sizeof fn_names / sizeof fn_names[0]; i++)
        if (fn_names[i].fn == f) return fn_names[i].name;
    return "(empty)";
}

/* 1. register: what a plugin list might say --- name, parent, own slots */
struct kind_desc { const char *name, *parent; bool abstract; slot own[SLOTS]; };
static const struct kind_desc descs[] = {
    { "gzip-file", "file",   false, { [WRITE] = gzip_write, [CLOSE] = gzip_close } },
    { "file",      "stream", false, { [WRITE] = file_write, [FLUSH] = file_flush } },
    { "memory",    "stream", false, { [WRITE] = memory_write } },
    { "broken",    "stream", false, { [CLOSE] = gzip_close } },       /* forgot write */
    { "stream",    nullptr,  true,
      { [FLUSH] = stream_flush_nothing, [CLOSE] = stream_close } },
};
#define KINDS (sizeof descs / sizeof descs[0])

static struct stream_ops *tables;       /* lives in its own page, so it can be sealed */
static bool built[KINDS], published[KINDS];
static size_t page;

static int find(const char *name)
{
    for (size_t i = 0; i < KINDS; i++)
        if (strcmp(descs[i].name, name) == 0) return (int)i;
    return -1;
}

/* 2. fill: the parent first (recursively), then copy it, then this kind's own slots */
static bool fill(int k, int depth)
{
    if (built[k]) return true;
    if (depth > (int)KINDS) return false;               /* a cycle in the parents */
    struct stream_ops t = {};
    if (descs[k].parent) {
        int p = find(descs[k].parent);
        if (p < 0 || !fill(p, depth + 1)) return false;
        t = tables[p];
    }
    for (int s = 0; s < SLOTS; s++)
        if (descs[k].own[s]) t.s[s] = descs[k].own[s];
    tables[k] = t;
    built[k] = true;
    return true;
}

int main(int argc, char **argv)
{
    bool attack = argc > 1 && strcmp(argv[1], "--write-after-seal") == 0;
    long ps = sysconf(_SC_PAGESIZE);
    page = ps > 0 ? (size_t)ps : 4096;
    if (KINDS * sizeof *tables > page) { puts("too many tables for a page"); return 1; }
    tables = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tables == MAP_FAILED) { puts("mmap failed"); return 1; }

    for (int k = 0; k < (int)KINDS; k++)
        if (!fill(k, 0)) { printf("cannot build %s\n", descs[k].name); return 1; }
    puts("built (defaults -> parent -> own slots):");
    for (size_t k = 0; k < KINDS; k++) {
        printf("  %-10s", descs[k].name);
        for (int s = 0; s < SLOTS; s++)
            printf(s + 1 < SLOTS ? " %s=%-20s" : " %s=%s",
                   slot_names[s], name_of(tables[k].s[s]));
        puts(descs[k].abstract ? "  (abstract)" : "");
    }

    /* 3. verify: a kind with an empty slot is not published, an abstract one never */
    for (size_t k = 0; k < KINDS; k++) {
        published[k] = !descs[k].abstract;
        for (int s = 0; s < SLOTS && published[k]; s++)
            if (!tables[k].s[s]) {
                printf("verify: %s has no %s --- not published\n",
                       descs[k].name, slot_names[s]);
                published[k] = false;
            }
    }

    /* 4. seal: from here on the tables are only read */
    if (mprotect(tables, page, PROT_READ) != 0) { puts("mprotect failed"); return 1; }
    const struct stream_ops *sealed = tables;
    puts("sealed: the page holding the tables is read-only");

    /* 5. use */
    for (size_t k = 0; k < KINDS; k++) {
        if (!published[k]) continue;
        struct stream st = { &sealed[k], descs[k].name };
        int r = st.ops->s[WRITE](&st) | st.ops->s[FLUSH](&st) | st.ops->s[CLOSE](&st);
        printf("use: a %s stream wrote, flushed and closed%s\n",
               st.kind, r ? " (error)" : "");
    }

    if (attack) {
        puts("now overwriting gzip-file's write slot...");
        fflush(stdout);
        tables[find("gzip-file")].s[WRITE] = memory_write;   /* read-only page */
        puts("the write went through");
    }
    munmap(tables, page);
    return 0;
}
