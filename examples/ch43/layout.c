/* 구조체의 배치 - 패딩, 재배열, 강제 정렬 */
#include <stddef.h>
#include <stdio.h>

struct loose  { char  a; int b; char c; };   /* 작은 것 사이에 큰 것 */
struct tight  { int   b; char a; char c; };  /* 큰 것부터 늘어놓기   */

#pragma pack(push, 1)                         /* 여기서부터 패딩 금지 */
struct packed { char  a; int b; char c; };
#pragma pack(pop)                             /* 원래 규칙으로 복귀   */

struct cacheline { alignas(64) int counter; };  /* 강제로 넓게 정렬 */

struct nested { struct loose inner; int tag; };

static void report(const char *name, size_t size, size_t align,
                   size_t oa, size_t ob, size_t oc)
{
    printf("%-8s 크기 %2zu  정렬 %2zu   오프셋 a=%zu b=%zu c=%zu\n",
           name, size, align, oa, ob, oc);
}

int main(void)
{
    report("loose",  sizeof(struct loose),  alignof(struct loose),
           offsetof(struct loose, a), offsetof(struct loose, b), offsetof(struct loose, c));
    report("tight",  sizeof(struct tight),  alignof(struct tight),
           offsetof(struct tight, a), offsetof(struct tight, b), offsetof(struct tight, c));
    report("packed", sizeof(struct packed), alignof(struct packed),
           offsetof(struct packed, a), offsetof(struct packed, b), offsetof(struct packed, c));

    printf("\ncacheline 크기 %zu  정렬 %zu\n",
           sizeof(struct cacheline), alignof(struct cacheline));
    printf("nested    크기 %zu  inner 오프셋 %zu  tag 오프셋 %zu\n",
           sizeof(struct nested), offsetof(struct nested, inner),
           offsetof(struct nested, tag));

    /* 멤버 세 개의 크기 합과 구조체 크기의 차이 = 패딩 바이트 수 */
    size_t members = sizeof(char) + sizeof(int) + sizeof(char);
    printf("\nloose: 멤버 합 %zu, 실제 %zu -> 패딩 %zu 바이트\n",
           members, sizeof(struct loose), sizeof(struct loose) - members);
    return 0;
}
