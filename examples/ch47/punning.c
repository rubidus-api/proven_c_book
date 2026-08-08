/* 같은 비트를 다른 타입으로 보는 세 가지 방법 — 그리고 각각의 계약. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

union bits { float f; uint32_t u; };

static void show(const char *how, uint32_t u)
{
    printf("  %-28s 0x%08X  (부호 %u, 지수 %3u, 가수 0x%06X)\n",
           how, u, u >> 31, (u >> 23) & 0xFFu, u & 0x7FFFFFu);
}

int main(void)
{
    float f = 1.5f;
    printf("float %.1f 의 비트를 32비트 정수로 읽는다\n\n", (double)f);

    /* ① 공용체 — C 에서는 허용된다.
         마지막에 쓴 멤버가 아닌 멤버를 읽으면 '표현을 다시 해석'한다. */
    union bits b = { .f = f };
    show("공용체로 읽기", b.u);

    /* ② memcpy — 어디서나 계약 안이다. 최적화하면 명령 하나로 접힌다. */
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    show("memcpy 로 옮기기", u);

    /* ③ 포인터 캐스트 — *이것만 계약 밖이다*(엄격한 앨리어싱, 37장).
         값이 같아 보여도 컴파일러가 순서를 바꿀 권리를 갖는다.
         아래 줄은 보이기 위해 적었을 뿐, 쓰면 안 되는 형태다. */
    puts("  포인터 캐스트로 읽기          *(uint32_t *)&f — 계약 밖이라 싣지 않는다");

    puts("\n[반대 방향도 같다]");
    union bits c = { .u = 0x40490FDBu };   /* 원주율에 가까운 비트 */
    printf("  0x40490FDB 를 float 로 보면 %.7f\n", (double)c.f);
    float g;
    memcpy(&g, &c.u, sizeof g);
    printf("  memcpy 로 옮겨도             %.7f\n", (double)g);

    puts("\n[C 와 C++ 가 갈리는 자리]");
    puts("  C   : 공용체의 다른 멤버를 읽는 것이 허용된다(표현을 다시 해석).");
    puts("  C++ : '활성 멤버'가 아닌 멤버를 읽는 것은 정의되지 않은 동작이다.");
    puts("  두 언어를 함께 쓰는 헤더에서는 memcpy 로 적는 편이 안전하다.");

    puts("\n[주의: 모든 비트열이 값이 되는 것은 아니다]");
    union bits nan_bits = { .u = 0x7FC00000u };
    printf("  0x7FC00000 → %f (NaN)\n", (double)nan_bits.f);
    puts("  정수·부동소수는 대개 무해하지만, 타입에 따라 '트랩 표현'일 수 있다.");
    return 0;
}
