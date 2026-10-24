/* 유연 배열 멤버의 자리와 크기 셈 --- 앞 멤버가 같아도 마지막 멤버의 타입이
   자리를 바꾼다. 두 구조체는 앞의 셋이 완전히 같고 마지막만 다르다. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* 유연 배열 멤버는 *자기 타입의 정렬*에 맞춰 앉는다.
   data 가 uint32_t(정렬 4)이므로 c 뒤에 빈자리 2바이트가 생긴다. */
struct s1 { uint16_t a; uint32_t b; uint16_t c; uint32_t data[]; };

/* data 가 char(정렬 1)이므로 c 바로 뒤에 붙는다 --- 빈자리가 없다. */
struct s2 { uint16_t a; uint32_t b; uint16_t c; char     data[]; };

/* 잡을 크기의 올바른 식: offsetof(구조체, data) + n * sizeof(data[0]) */
#define NEED(type, member, n) (offsetof(type, member) + (n) * sizeof(((type *)0)->member[0]))

static void layout(const char *name, size_t a, size_t b, size_t c,
                   size_t data, size_t size, size_t align, size_t elem)
{
    printf("  %s: a=%zu b=%zu c=%zu data=%zu | sizeof=%zu alignof=%zu"
           " sizeof(data[0])=%zu\n", name, a, b, c, data, size, align, elem);
    printf("      padding before data: %zu byte(s)\n", data - (c + sizeof(uint16_t)));
}

int main(void)
{
    puts("(1) the same three members, a different last member");
    layout("s1", offsetof(struct s1, a), offsetof(struct s1, b),
           offsetof(struct s1, c), offsetof(struct s1, data),
           sizeof(struct s1), alignof(struct s1), sizeof(((struct s1 *)0)->data[0]));
    layout("s2", offsetof(struct s2, a), offsetof(struct s2, b),
           offsetof(struct s2, c), offsetof(struct s2, data),
           sizeof(struct s2), alignof(struct s2), sizeof(((struct s2 *)0)->data[0]));

    puts("\n  the flexible array member sits at its own type's alignment:");
    puts("    s1: data is uint32_t (align 4) -> two bytes of padding appear before it");
    puts("    s2: data is char     (align 1) -> it follows c directly, no padding");
    puts("  both structs have the same sizeof, and it says nothing about where data is.");

    puts("\n(2) how many bytes to allocate for n elements");
    puts("      n | s1: offsetof rule | sizeof rule | s2: offsetof rule | sizeof rule");
    for (size_t n = 0; n <= 3; n++)
        printf("     %2zu | %17zu | %11zu | %17zu | %11zu\n", n,
               NEED(struct s1, data, n), sizeof(struct s1) + n * 4,
               NEED(struct s2, data, n), sizeof(struct s2) + n * 1);
    puts("  for s1 the two rules agree by accident (offsetof == sizeof == 12).");
    puts("  for s2 they never agree: the sizeof rule counts the tail padding twice.");

    puts("\n(3) the same arithmetic, run backwards: how many elements are in a buffer?");
    size_t buf = NEED(struct s2, data, 3);      /* 세 원소가 정확히 들어가는 크기 */
    printf("  a %zu-byte s2 record holds exactly 3 elements\n", buf);
    printf("  offsetof rule: (%zu - %zu) / 1 = %zu   <- right\n",
           buf, offsetof(struct s2, data), buf - offsetof(struct s2, data));
    printf("  sizeof   rule: (%zu - %zu) / 1 = %zu   <- two elements lost\n",
           buf, sizeof(struct s2), buf - sizeof(struct s2));

    puts("\n(4) is an empty record well formed?");
    printf("  an empty s2 record is %zu bytes on the wire\n", NEED(struct s2, data, 0));
    printf("  \"len >= offsetof(data)\" -> %s   <- right\n",
           NEED(struct s2, data, 0) >= offsetof(struct s2, data) ? "accepted" : "REJECTED");
    printf("  \"len >= sizeof(struct)\" -> %s  <- a valid record thrown away\n",
           NEED(struct s2, data, 0) >= sizeof(struct s2) ? "accepted" : "REJECTED");

    puts("\n(5) the price of the exact fit");
    printf("  s2 with one element needs %zu bytes, but sizeof(struct s2) is %zu\n",
           NEED(struct s2, data, 1), sizeof(struct s2));
    puts("  so the object is smaller than its own type: *a = *b, memcpy(a, b, sizeof *a),");
    puts("  or passing it by value would run past the end of the allocation.");
    puts("  a struct with a flexible array member is copied field by field, never whole.");

    /* 실제로 잡아 보고, 마지막 원소가 어디서 끝나는지 눈으로 확인한다 */
    size_t n = 3;
    struct s2 *r = malloc(NEED(struct s2, data, n));
    if (!r) { perror("malloc"); return 1; }
    r->a = 1; r->b = 2; r->c = 3;
    for (size_t i = 0; i < n; i++) r->data[i] = (char)('A' + i);
    printf("\n  allocated %zu bytes; data[%zu] ends at offset %zu; data = %.3s\n",
           NEED(struct s2, data, n), n - 1,
           offsetof(struct s2, data) + n * sizeof r->data[0], r->data);
    free(r);
    return 0;
}
