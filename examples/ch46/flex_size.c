/* 유연 배열 멤버의 크기 셈 — `sizeof` 와 `offsetof` 는 언제 같고 언제 갈리는가. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ① 갈리지 않는 모양 — 앞 멤버가 남기는 꼬리 패딩이 없다 */
struct plain {
    uint32_t len;
    char     data[];
};

/* ② 갈리는 모양 — 구조체의 정렬(8)이 마지막 멤버의 자리(12)보다 크다.
      그래서 sizeof 는 16 으로 올림되고, 그 4바이트가 '꼬리 패딩'이다. */
struct rec {
    double   stamp;
    uint32_t count;
    uint16_t data[];
};

/* 두 규칙이 내놓는 '잡을 크기' */
static size_t by_offsetof(size_t n) { return offsetof(struct rec, data) + n * sizeof(uint16_t); }
static size_t by_sizeof(size_t n)   { return sizeof(struct rec)         + n * sizeof(uint16_t); }

int main(void)
{
    puts("(1) when the two rules agree");
    printf("  struct plain: offsetof(data) = %zu, sizeof = %zu  -> same\n",
           offsetof(struct plain, data), sizeof(struct plain));

    puts("\n(2) when they do not");
    printf("  struct rec:   offsetof(data) = %zu, sizeof = %zu, alignof = %zu\n",
           offsetof(struct rec, data), sizeof(struct rec), alignof(struct rec));
    printf("  tail padding  = sizeof - offsetof(data) = %zu bytes\n",
           sizeof(struct rec) - offsetof(struct rec, data));

    puts("\n(3) bytes reserved for n elements");
    puts("   n | offsetof rule | sizeof rule | wasted");
    for (size_t n = 0; n <= 4; n++)
        printf("  %2zu | %13zu | %11zu | %6zu\n",
               n, by_offsetof(n), by_sizeof(n), by_sizeof(n) - by_offsetof(n));

    /* 여기까지는 '조금 더 잡는다'는 이야기다. 표준이 보장하기를
       sizeof >= offsetof 이므로 sizeof 규칙이 *모자라는* 일은 없다.
       갈림이 결과를 바꾸는 것은 크기를 *거꾸로* 되물을 때다. */

    puts("\n(4) the same arithmetic, run backwards: how many elements fit?");
    size_t buf = by_offsetof(3);          /* 세 원소가 정확히 들어가는 크기 */
    printf("  a buffer of %zu bytes holds exactly 3 elements\n", buf);
    printf("  offsetof rule says (%zu - %zu) / %zu = %zu   <- right\n",
           buf, offsetof(struct rec, data), sizeof(uint16_t),
           (buf - offsetof(struct rec, data)) / sizeof(uint16_t));
    printf("  sizeof   rule says (%zu - %zu) / %zu = %zu   <- two elements lost\n",
           buf, sizeof(struct rec), sizeof(uint16_t),
           (buf - sizeof(struct rec)) / sizeof(uint16_t));

    puts("\n(5) is an empty record well formed?");
    size_t empty = by_offsetof(0);
    printf("  an empty record on the wire is %zu bytes\n", empty);
    printf("  \"len >= offsetof(data)\" -> %s   <- right\n",
           empty >= offsetof(struct rec, data) ? "accepted" : "REJECTED");
    printf("  \"len >= sizeof(struct)\" -> %s  <- a valid record thrown away\n",
           empty >= sizeof(struct rec) ? "accepted" : "REJECTED");

    puts("\n(6) the price of the exact fit");
    printf("  offsetof rule for n=1 reserves %zu bytes, but sizeof(struct rec) is %zu\n",
           by_offsetof(1), sizeof(struct rec));
    puts("  so the object is smaller than its own type: *a = *b, memcpy(a, b, sizeof *a),");
    puts("  or passing it by value would run past the end of the allocation.");
    puts("  a struct with a flexible array member is copied field by field, never whole.");

    /* 실제로 잡아 보고, 마지막 원소가 어디서 끝나는지 눈으로 확인한다 */
    size_t n = 3;
    struct rec *r = malloc(by_offsetof(n));
    if (!r) { perror("malloc"); return 1; }
    r->stamp = 1.5;
    r->count = (uint32_t)n;
    for (size_t i = 0; i < n; i++) r->data[i] = (uint16_t)(10 * (i + 1));

    printf("\n  allocated %zu bytes; data[%zu] ends at offset %zu\n",
           by_offsetof(n), n - 1,
           offsetof(struct rec, data) + n * sizeof(uint16_t));
    printf("  data = %u %u %u\n", r->data[0], r->data[1], r->data[2]);
    free(r);
    return 0;
}
