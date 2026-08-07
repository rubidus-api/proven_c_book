/* <stdint.h> 의 세 갈래와 uint8_t 의 함정. */
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    puts("[세 갈래 — 무엇을 요구하느냐가 다르다]");
    printf("  %-16s %-6s %s\n", "타입", "크기", "무엇을 보장하는가");
    printf("  %-16s %-6zu %s\n", "uint8_t",       sizeof(uint8_t),
           "정확히 8비트, 패딩 없음 (선택 사항)");
    printf("  %-16s %-6zu %s\n", "uint_least8_t", sizeof(uint_least8_t),
           "8비트 이상 중 가장 작은 것 (필수)");
    printf("  %-16s %-6zu %s\n", "uint_fast8_t",  sizeof(uint_fast8_t),
           "8비트 이상 중 대개 가장 빠른 것 (필수)");
    printf("  %-16s %-6zu %s\n", "uint_least32_t", sizeof(uint_least32_t), "32비트 이상");
    printf("  %-16s %-6zu %s\n", "uint_fast32_t",  sizeof(uint_fast32_t),  "32비트 이상, 속도 우선");
    printf("  %-16s %-6zu %s\n", "uintmax_t",      sizeof(uintmax_t),      "가장 넓은 정수 (필수)");
    printf("  %-16s %-6zu %s\n", "uintptr_t",      sizeof(uintptr_t),      "void* 왕복 (선택 사항)");

    puts("\n[함정 ① uint8_t 는 대개 unsigned char 라 '문자'로 샌다]");
    uint8_t age = 65;
    printf("  %%u 로: %u\n", age);
    printf("  %%c 로: %c   ← 65 가 아니라 'A' 가 나온다\n", age);
    printf("  putchar(age) 도 같은 함정이다\n");

    puts("\n[함정 ② 산술을 하면 int 로 승격된다 — 8비트 산술이 아니다]");
    uint8_t a = 200, b = 100;
    printf("  a + b        = %d   ← int 로 계산되어 300 이다(감아 돌지 않았다)\n", a + b);
    printf("  (uint8_t)(a+b) = %u   ← 다시 담아야 감아 돈다\n", (uint8_t)(a + b));
    printf("  sizeof(a + b) = %zu  ← 결과는 4바이트다\n", sizeof(a + b));

    puts("\n[함정 ③ 별명일 뿐이라 서식도 별명을 따라야 한다]");
    printf("  PRIu8 을 쓰면: %" PRIu8 "\n", age);
    printf("  UINT8_C(200) 는 값 %u, UINT8_MAX 는 %u\n", UINT8_C(200), UINT8_MAX);

    puts("\n[언제 무엇을 쓰는가]");
    puts("  프로토콜·파일 형식·레지스터 → 정확 폭 (uint32_t)");
    puts("  이식성이 먼저, 폭은 최소만  → 최소 폭 (uint_least16_t)");
    puts("  반복 카운터·지역 계산       → 가장 빠른 폭 (uint_fast32_t)");
    puts("  크기·인덱스·바이트 수       → size_t (<stddef.h>)");
    return 0;
}
