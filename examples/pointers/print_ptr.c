/* 주소를 사람이 읽는 형태로 — %p 의 계약과 그 대안들.
   ★ 주소 자체는 실행마다 달라지므로(ASLR) 이 시연은 '값'이 아니라
      '성질'만 인쇄한다. 지면에 실린 출력이 매번 같아야 하기 때문이다. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    int n = 42;

    /* ① %p 에 넘길 수 있는 것은 void* (그리고 C23 부터 문자 타입 포인터)뿐이다.
          다른 포인터는 캐스트한다 — 습관으로 삼는 편이 낫다. */
    puts("[the contract of %p]");
    printf("  a null pointer through %%p: %p   <- the shape of the output is up to the implementation\n",
           (void *)nullptr);
    puts("  other addresses vary per run, so they are not printed here.");

    /* ② 같은 주소를 두 가지로 찍어 문자열이 같은지 견준다 */
    char a[64], b[64];
    snprintf(a, sizeof a, "%p", (void *)&n);
    snprintf(b, sizeof b, "%#" PRIxPTR, (uintptr_t)(void *)&n);
    printf("\n[do %%p and PRIxPTR print the same characters?]\n");
    printf("  on this implementation: %s\n", strcmp(a, b) == 0 ? "same" : "different");
    printf("  same number of characters: %s (the address value is not shown)\n",
           strlen(a) == strlen(b) ? "yes" : "no");

    /* ③ 왕복이 보장되는 것은 void* ↔ uintptr_t 다 */
    uintptr_t u = (uintptr_t)(void *)&n;
    printf("\n[uintptr_t round trip]\n");
    printf("  is the restored pointer the same as the original: %s\n",
           (void *)u == (void *)&n ? "yes - the standard promises it" : "no");

    /* ④ 폭을 고정하면 로그가 가지런해진다 (자리는 0 으로 채운다) */
    char line[80];
    snprintf(line, sizeof line, "obj=0x%016" PRIxPTR, u);
    printf("\n[building one log line]\n");
    printf("  length of the line built: %zu characters (constant, whatever the address)\n",
           strlen(line));

    /* ⑤ 배치를 견줄 때는 같은 배열 안에서 뺀다 — 서로 다른 객체끼리는 계약 밖 */
    int arr[3];
    printf("\n[comparing the layout]\n");
    printf("  arr[0]->arr[1] distance: %td bytes (legal, same array)\n",
           (char *)&arr[1] - (char *)&arr[0]);
    printf("  same as sizeof(int): %s\n",
           (size_t)((char *)&arr[1] - (char *)&arr[0]) == sizeof(int) ? "yes" : "no");

    /* ⑥ 사람이 볼 로그에는 주소보다 이름·순번이 낫다 */
    puts("\n[a label instead of an address]");
    static const struct { const char *name; const void *at; } table[] = {
        { "n", nullptr }, { "arr", nullptr },
    };
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        printf("  #%zu %-4s <- this is the name the log keeps\n", i, table[i].name);
    puts("  an address means something only within that run. The next run gives another number.");
    return 0;
}
