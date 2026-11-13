/* VBR 의 첫 섹터에 드는 부분. 나머지 섹터를 0x7e00 부터 이어 싣고 그리로 넘어간다.
   512바이트에 다 들지 않는 다음 단계를 이렇게 나누어 싣는 것은 큰 부트로더도 같다. */
#include "bios.h"

extern uint8_t _load_end[];        /* 링커 스크립트가 정한다: 실을 것의 끝 */
[[noreturn]] void vbr_main(uint8_t drive, const struct part *entry);

[[noreturn]] void vbr_boot(uint8_t drive, const struct part *entry)
{
    uint8_t *at = load_area() + 512;
    for (uint32_t s = 1; at < _load_end; s++, at += 512)
        if (disk_read(drive, entry->lba + s, at) != 0) stop(1);
    vbr_main(drive, entry);
}
