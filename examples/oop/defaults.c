/* 표의 빈칸 --- 두 관례를 나란히 놓는다.
   관례 1: 할 일이 없는 칸은 NULL 로 비워 두고, 부르는 쪽이 칸마다 확인한다.
   관례 2: 기본 행동을 함수로 두고 「기본값 묶음」 매크로로 모든 칸을 먼저 채운 뒤,
           종류마다 다른 칸만 덮어쓴다(N. Brown, LWN 2011). 부르는 쪽은 그냥 부른다.
   스트림 두 종류: 「파일」은 쓴 것을 모았다가 flush 때 내보내고, 「메모리」는 바로 적으니
   flush 할 일이 없다. close 는 표에 *나중에 더한* 칸이라고 치자. */
#include <stdio.h>

struct stream;
struct stream_ops {
    void (*write)(struct stream *s, const char *text);
    void (*flush)(struct stream *s);
    void (*close)(struct stream *s);    /* 나중에 더한 칸 */
};
struct stream { const struct stream_ops *ops; const char *name; int pending; };

static void file_write(struct stream *s, const char *text)
{
    (void)text;
    s->pending++;                       /* 모아 두기만 한다 */
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

/* ---- 관례 1: 빈칸 ---- */
static const struct stream_ops file_ops_1   = { .write = file_write, .flush = file_flush };
static const struct stream_ops memory_ops_1 = { .write = memory_write };
/* 두 표 모두 close 를 모른다 --- 칸이 생기기 전에 쓴 표라 NULL 이다 */

static void finish_1(struct stream *s)
{
    if (s->ops->flush)                  /* 부르는 쪽마다 칸마다 이 확인 */
        s->ops->flush(s);
    if (s->ops->close)
        s->ops->close(s);
}

/* ---- 관례 2: 기본값 묶음 ---- */
static void stream_flush_nothing(struct stream *s) { (void)s; }
static void stream_close_default(struct stream *s)
{
    printf("  %s: closed (default close)\n", s->name);
}
#define STREAM_OPS_DEFAULTS \
    .flush = stream_flush_nothing, .close = stream_close_default

/* 같은 칸을 두 번 초기화하면 뒤의 것이 이긴다(C23 §6.7.11p20). 그러나 컴파일러는 이것을
   실수로 두 번 쓴 칸과 구별하지 못해 경고한다(GCC -Wextra 의 -Woverride-init,
   Clang 의 -Winitializer-overrides). 표를 선언하는 이 몇 줄에서만 끈다. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
static const struct stream_ops file_ops_2 = {
    STREAM_OPS_DEFAULTS,
    .write = file_write,
    .flush = file_flush,                /* 기본값을 덮어쓴다 */
};
static const struct stream_ops memory_ops_2 = {
    STREAM_OPS_DEFAULTS,
    .write = memory_write,              /* flush · close 는 기본 그대로 */
};
#pragma GCC diagnostic pop

static void finish_2(struct stream *s)
{
    s->ops->flush(s);                   /* 빈칸이 없으니 확인도 없다 */
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
