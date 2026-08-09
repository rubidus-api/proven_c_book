/* 멤버 크기를 더한 값과 구조체의 크기는 왜 다른가 — 배치를 직접 인쇄한다. */
#include <stddef.h>
#include <stdio.h>

/* 같은 멤버, 순서만 다르다 */
struct loose { char  a; int   b; char  c; };   /* 작은 것 사이에 큰 것 */
struct tight { int   b; char  a; char  c; };   /* 큰 것부터 */

int main(void)
{
    printf("sum of the member sizes: %zu + %zu + %zu = %zu bytes\n",
           sizeof(char), sizeof(int), sizeof(char),
           sizeof(char) * 2 + sizeof(int));

    printf("\nstruct loose { char a; int b; char c; }\n");
    printf("  sizeof  = %zu, _Alignof = %zu\n",
           sizeof(struct loose), alignof(struct loose));
    printf("  offsetof(a) = %zu, offsetof(b) = %zu, offsetof(c) = %zu\n",
           offsetof(struct loose, a), offsetof(struct loose, b),
           offsetof(struct loose, c));

    printf("\nstruct tight { int b; char a; char c; }\n");
    printf("  sizeof  = %zu, _Alignof = %zu\n",
           sizeof(struct tight), alignof(struct tight));
    printf("  offsetof(b) = %zu, offsetof(a) = %zu, offsetof(c) = %zu\n",
           offsetof(struct tight, b), offsetof(struct tight, a),
           offsetof(struct tight, c));

    /* 배치를 그림처럼 그려 본다: 멤버가 차지한 칸은 이름으로, 빈자리는 . 으로 */
    puts("\nlayout cell by cell (numbers are offsets, dots are padding):");
    for (size_t i = 0; i < sizeof(struct loose); i++) {
        char mark = '.';
        if (i == offsetof(struct loose, a)) mark = 'a';
        else if (i >= offsetof(struct loose, b)
              && i <  offsetof(struct loose, b) + sizeof(int)) mark = 'b';
        else if (i == offsetof(struct loose, c)) mark = 'c';
        printf("%c", mark);
    }
    printf("   <- loose (%zu bytes)\n", sizeof(struct loose));
    for (size_t i = 0; i < sizeof(struct tight); i++) {
        char mark = '.';
        if (i < sizeof(int)) mark = 'b';
        else if (i == offsetof(struct tight, a)) mark = 'a';
        else if (i == offsetof(struct tight, c)) mark = 'c';
        printf("%c", mark);
    }
    printf("       <- tight (%zu bytes)\n", sizeof(struct tight));

    /* 배열로 늘어놓으면 차이가 곱해진다 */
    printf("\nwith a million elements: loose %zu MiB, tight %zu MiB\n",
           sizeof(struct loose) * 1000000u / (1024 * 1024),
           sizeof(struct tight) * 1000000u / (1024 * 1024));
    return 0;
}
