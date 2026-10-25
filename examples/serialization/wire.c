/* 직렬화를 정직하게 건너뛰기: 기억 속 배치가 *곧* 형식일 때.
   고정 폭 멤버, 넓은 것부터, 숨은 패딩 없음, 오프셋은 static_assert 로 못박는다 ---
   그래서 다른 컴파일러나 옵션이면 다른 바이트를 조용히 쓰는 대신 빌드가 멈춘다.
   바이트 순서는 여전히 기계의 것이다: 그것도 확인한다. */
#include <stdbit.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct wire_header {            /* 같은 기계의 두 프로세스가 나눠 쓴다 */
    uint64_t sequence;
    uint32_t length;
    uint16_t kind;
    uint8_t  flags;
    uint8_t  version;
};
static_assert(sizeof(struct wire_header) == 16, "layout changed");
static_assert(offsetof(struct wire_header, length)  == 8);
static_assert(offsetof(struct wire_header, kind)    == 12);
static_assert(offsetof(struct wire_header, flags)   == 14);
static_assert(offsetof(struct wire_header, version) == 15);
static_assert(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__,
              "this format is little endian; a big-endian build must convert");

struct careless {               /* 같은 멤버를 아무렇게나 늘어놓은 것 */
    uint8_t  flags;
    uint64_t sequence;
    uint16_t kind;
    uint32_t length;
    uint8_t  version;
};

int main(void)
{
    printf("wire_header: %zu bytes, no padding (8 + 4 + 2 + 1 + 1)\n",
           sizeof(struct wire_header));
    printf("careless   : %zu bytes for the same 16 bytes of members\n",
           sizeof(struct careless));
    printf("  offsets: flags %zu, sequence %zu, kind %zu, length %zu, version %zu\n",
           offsetof(struct careless, flags), offsetof(struct careless, sequence),
           offsetof(struct careless, kind), offsetof(struct careless, length),
           offsetof(struct careless, version));
    return 0;
}
