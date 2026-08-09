/* 통째로 0으로 — { 0 } 과 C23 의 { } 는 포인터 멤버까지 널로 만든다. */
#include <stdio.h>
#include <string.h>

struct inner { int k; char *note; };

struct config {
    int          retries;
    char        *path;      /* 포인터 멤버 */
    double       ratio;
    struct inner in;        /* 안에 또 포인터가 있다 */
    char         name[4];
};

static void dump(const char *tag, const struct config *c)
{
    printf("%s retries=%d path=%s ratio=%g in.k=%d in.note=%s name[0]=%d\n",
           tag, c->retries,
           c->path == NULL ? "null" : "not null",
           c->ratio, c->in.k,
           c->in.note == NULL ? "null" : "not null",
           c->name[0]);
}

static void bytes(const char *tag, const void *p, size_t n)
{
    const unsigned char *b = p;
    size_t zero = 0;
    for (size_t i = 0; i < n; i++)
        zero += (b[i] == 0);
    printf("%s %zu bytes, %zu of them zero\n", tag, n, zero);
}

int main(void)
{
    struct config a = {0};      /* 첫 멤버만 명시, 나머지는 기본 초기화 */
    struct config b = {};       /* C23: 빈 초기자 — 객체 전체가 기본 초기화 */

    dump("{0} :", &a);
    dump("{ } :", &b);

    /* 지정 초기화도 마찬가지다 — 적지 않은 멤버는 기본 초기화된다 */
    struct config c = { .retries = 3 };
    dump("{.retries=3}:", &c);

    /* memset 은 '모든 비트 0' 이지 '널'이 아니다 — 이 구현에서는 같지만
       표준이 같다고 보장하지 않는다. */
    struct config m;
    memset(&m, 0, sizeof m);
    printf("is path null after memset: %s (on this implementation)\n",
           m.path == NULL ? "yes" : "no");

    printf("\nsizeof(struct config) = %zu, sum of member sizes = %zu - the difference is padding\n",
           sizeof(struct config),
           sizeof(int) + sizeof(char *) + sizeof(double)
           + sizeof(struct inner) + 4);
    bytes("{0} :", &a, sizeof a);
    bytes("{ } :", &b, sizeof b);
    return 0;
}
