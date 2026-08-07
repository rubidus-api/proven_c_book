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
    puts("[%p 의 계약]");
    printf("  널 포인터를 %%p 로: %p   ← 출력 모양은 구현이 정한다\n",
           (void *)nullptr);
    puts("  그 밖의 주소는 실행마다 달라 여기 싣지 않는다.");

    /* ② 같은 주소를 두 가지로 찍어 문자열이 같은지 견준다 */
    char a[64], b[64];
    snprintf(a, sizeof a, "%p", (void *)&n);
    snprintf(b, sizeof b, "%#" PRIxPTR, (uintptr_t)(void *)&n);
    printf("\n[%%p 와 PRIxPTR 이 같은 글자를 내는가]\n");
    printf("  이 구현에서: %s\n", strcmp(a, b) == 0 ? "같다" : "다르다");
    printf("  글자 수가 같은가: %s (주소 값은 싣지 않는다)\n",
           strlen(a) == strlen(b) ? "예" : "아니오");

    /* ③ 왕복이 보장되는 것은 void* ↔ uintptr_t 다 */
    uintptr_t u = (uintptr_t)(void *)&n;
    printf("\n[uintptr_t 왕복]\n");
    printf("  되돌린 포인터가 원래와 같은가: %s\n",
           (void *)u == (void *)&n ? "예 — 표준이 약속한다" : "아니오");

    /* ④ 폭을 고정하면 로그가 가지런해진다 (자리는 0 으로 채운다) */
    char line[80];
    snprintf(line, sizeof line, "obj=0x%016" PRIxPTR, u);
    printf("\n[로그 한 줄로 만들기]\n");
    printf("  만들어진 줄의 길이: %zu글자 (주소 값과 무관하게 일정하다)\n",
           strlen(line));

    /* ⑤ 배치를 견줄 때는 같은 배열 안에서 뺀다 — 서로 다른 객체끼리는 계약 밖 */
    int arr[3];
    printf("\n[배치를 견주기]\n");
    printf("  arr[0]→arr[1] 간격: %td바이트 (같은 배열 안이라 합법)\n",
           (char *)&arr[1] - (char *)&arr[0]);
    printf("  sizeof(int) 와 같은가: %s\n",
           (size_t)((char *)&arr[1] - (char *)&arr[0]) == sizeof(int) ? "예" : "아니오");

    /* ⑥ 사람이 볼 로그에는 주소보다 이름·순번이 낫다 */
    puts("\n[주소 대신 이름표]");
    static const struct { const char *name; const void *at; } table[] = {
        { "n", nullptr }, { "arr", nullptr },
    };
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        printf("  #%zu %-4s ← 로그에는 이 이름이 남는다\n", i, table[i].name);
    puts("  주소는 그 실행 안에서만 뜻이 있다. 다음 실행에서는 다른 수가 된다.");
    return 0;
}
