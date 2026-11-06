/* 표를 실행 때 다섯 단계로 짓는다: 등록, 채우기, 검증, 봉인, 사용.
   종류는 `static const` 표가 아니라 자료로 들어온다(플러그인 목록이 그렇듯이).
   채우기는 기본값 -> 부모 -> 자기 칸 순서라, 종류는 다른 것만 적는다.
   봉인은 표가 든 쪽을 읽기 전용으로 바꾼다(POSIX mprotect).
   --write-after-seal 을 주면 봉인 뒤에 칸 하나를 고쳐 본다. */
#define _DEFAULT_SOURCE        /* MAP_ANONYMOUS: 리눅스와 BSD, POSIX 2024 */
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

/* 행동들 --- 저마다 자기가 누구인지만 말한다 */
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

/* 1. 등록: 플러그인 목록이 적을 만한 것 --- 이름, 부모, 자기 칸 */
struct kind_desc { const char *name, *parent; bool abstract; slot own[SLOTS]; };
static const struct kind_desc descs[] = {
    { "gzip-file", "file",   false, { [WRITE] = gzip_write, [CLOSE] = gzip_close } },
    { "file",      "stream", false, { [WRITE] = file_write, [FLUSH] = file_flush } },
    { "memory",    "stream", false, { [WRITE] = memory_write } },
    { "broken",    "stream", false, { [CLOSE] = gzip_close } },       /* write 빠짐 */
    { "stream",    nullptr,  true,
      { [FLUSH] = stream_flush_nothing, [CLOSE] = stream_close } },
};
#define KINDS (sizeof descs / sizeof descs[0])

static struct stream_ops *tables;       /* 따로 한 쪽에 둔다 --- 봉인할 수 있게 */
static bool built[KINDS], published[KINDS];
static size_t page;

static int find(const char *name)
{
    for (size_t i = 0; i < KINDS; i++)
        if (strcmp(descs[i].name, name) == 0) return (int)i;
    return -1;
}

/* 2. 채우기: 부모 먼저(재귀로) → 복사 → 자기 칸 */
static bool fill(int k, int depth)
{
    if (built[k]) return true;
    if (depth > (int)KINDS) return false;               /* 부모가 돌고 돈다 */
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

    /* 3. 검증: 빈칸이 있는 종류는 내놓지 않는다. 추상 종류는 늘 */
    for (size_t k = 0; k < KINDS; k++) {
        published[k] = !descs[k].abstract;
        for (int s = 0; s < SLOTS && published[k]; s++)
            if (!tables[k].s[s]) {
                printf("verify: %s has no %s --- not published\n",
                       descs[k].name, slot_names[s]);
                published[k] = false;
            }
    }

    /* 4. 봉인: 여기부터 표는 읽기만 한다 */
    if (mprotect(tables, page, PROT_READ) != 0) { puts("mprotect failed"); return 1; }
    const struct stream_ops *sealed = tables;
    puts("sealed: the page holding the tables is read-only");

    /* 5. 사용 */
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
        tables[find("gzip-file")].s[WRITE] = memory_write;   /* 읽기 전용 쪽 */
        puts("the write went through");
    }
    munmap(tables, page);
    return 0;
}
