/* PCI 열거. 펌웨어(U-Boot · EDK2)와 운영체제가 부팅 때 하는 일을 줄여 옮겼다.
   하드웨어에 닿는 것은 hw_read32 · hw_write32 뿐이다. */
#include "pci.h"
#include "hw.h"

// [excerpt ecam]
/* ECAM: 버스 · 장치 · 기능 · 레지스터 번호를 비트 자리에 놓으면 그대로 주소가 된다.
   기능 하나에 4 KiB, 장치 하나에 기능 여덟, 버스 하나에 장치 서른둘. */
static uintptr_t cfg_addr(const struct pci_host *h, unsigned bus, unsigned dev,
                          unsigned fn, unsigned off)
{
    return h->ecam + ((uintptr_t)bus << 20 | (uintptr_t)dev << 15
                      | (uintptr_t)fn << 12 | off);
}
// [/excerpt]

uint32_t pci_read32(const struct pci_host *h, unsigned bus, unsigned dev, unsigned fn,
                    unsigned off)
{ return hw_read32(cfg_addr(h, bus, dev, fn, off)); }

void pci_write32(const struct pci_host *h, unsigned bus, unsigned dev, unsigned fn,
                 unsigned off, uint32_t v)
{ hw_write32(cfg_addr(h, bus, dev, fn, off), v); }

enum { ID = 0x00, CMD = 0x04, CLASS = 0x08, HDR = 0x0C, BAR0 = 0x10 };
enum { CMD_IO = 0x0001, CMD_MEM = 0x0002, CMD_MASTER = 0x0004 };

// [excerpt sizing]
/* BAR 의 크기는 장치에게 물어볼 수 없다. 전부 1 을 써 보고, 장치가 0 으로 굳혀 둔
   아래 자리를 되읽어 알아낸다. 64비트 BAR 는 다음 칸이 위 32비트다. */
static unsigned size_bar(struct pci_host *h, struct pci_fn *f, unsigned i)
{
    unsigned off = BAR0 + 4 * i;
    uint32_t orig = pci_read32(h, f->bus, f->dev, f->fn, off);
    if (orig & 1) return 1;                         /* 입출력 공간 BAR 는 건너뛴다 */
    int is64 = (orig >> 1 & 3) == 2;

    pci_write32(h, f->bus, f->dev, f->fn, off, 0xFFFFFFFFu);
    uint64_t mask = pci_read32(h, f->bus, f->dev, f->fn, off) & ~0xFull;
    if (is64) {
        pci_write32(h, f->bus, f->dev, f->fn, off + 4, 0xFFFFFFFFu);
        mask |= (uint64_t)pci_read32(h, f->bus, f->dev, f->fn, off + 4) << 32;
    } else {
        mask |= 0xFFFFFFFF00000000ull;
    }
    if ((uint32_t)mask == 0) {                      /* 굳힌 비트가 없다: BAR 가 없다 */
        pci_write32(h, f->bus, f->dev, f->fn, off, orig);
        return is64 ? 2 : 1;
    }
    uint64_t size = ~mask + 1;                      /* 굳힌 자리의 가장 낮은 1 */

    /* 넷째 걸음: BAR 에 주소를 되돌려 쓴다. 펌웨어인 여기서는 크기로 줄을 맞춰 창에서
       떼어 준 새 주소를 쓴다(운영체제라면 펌웨어가 준 원래 값을 되돌린다).
       이 주소가 곧 장치 레지스터의 주소가 된다. */
    uint64_t at = (h->mem_next + size - 1) & ~(size - 1);
    if (at + size > h->mem_end) at = 0;             /* 창이 모자라면 주지 않는다 */
    else h->mem_next = at + size;
    pci_write32(h, f->bus, f->dev, f->fn, off, (uint32_t)at);
    if (is64) pci_write32(h, f->bus, f->dev, f->fn, off + 4, (uint32_t)(at >> 32));
    f->bar[i] = at;
    f->bar_size[i] = size;
    return is64 ? 2 : 1;
}
// [/excerpt]

// [excerpt scan]
int pci_scan(struct pci_host *h,
             void (*visit)(const struct pci_host *, const struct pci_fn *))
{
    int found = 0;
    for (unsigned dev = 0; dev < 32; dev++) {
        for (unsigned fn = 0; fn < 8; fn++) {
            uint32_t id = pci_read32(h, 0, dev, fn, ID);
            if ((id & 0xFFFF) == 0xFFFF) {          /* 아무도 대답하지 않으면 전부 1 */
                if (fn == 0) break;
                continue;
            }
            struct pci_fn f = { .bus = 0, .dev = dev, .fn = fn,
                                .vendor = (uint16_t)id, .device = (uint16_t)(id >> 16),
                                .class_code = pci_read32(h, 0, dev, fn, CLASS) >> 8 };
            uint32_t hdr = pci_read32(h, 0, dev, fn, HDR) >> 16 & 0xFF;
            uint32_t cmd = pci_read32(h, 0, dev, fn, CMD) & 0xFFFF;
            /* 크기를 재는 동안 BAR 에는 엉뚱한 값이 들어간다. 그동안 장치가 그 주소에
               대답하지 않도록 기억 · 입출력 해독을 꺼 둔다. */
            pci_write32(h, 0, dev, fn, CMD, cmd & ~(uint32_t)(CMD_MEM | CMD_IO));
            if ((hdr & 0x7F) == 0)                  /* 보통 장치(다리가 아니다) */
                for (unsigned i = 0; i < 6; )
                    i += size_bar(h, &f, i);
            pci_write32(h, 0, dev, fn, CMD, cmd | CMD_MEM | CMD_MASTER);
            visit(h, &f);
            found++;
            if (fn == 0 && !(hdr & 0x80)) break;    /* 기능이 하나뿐인 장치 */
        }
    }
    return found;
}
// [/excerpt]

// [excerpt caps]
/* 능력 목록: 상태 레지스터의 비트 4 가 서 있으면 0x34 에 첫 항목의 자리가 있다.
   항목마다 [종류 1바이트][다음 자리 1바이트] 로 시작하는 연결 리스트다. */
unsigned pci_caps(const struct pci_host *h, const struct pci_fn *f, uint8_t *ids,
                  unsigned max)
{
    if (!(pci_read32(h, f->bus, f->dev, f->fn, CMD) >> 16 & 0x10)) return 0;
    unsigned n = 0;
    unsigned at = pci_read32(h, f->bus, f->dev, f->fn, 0x34) & 0xFC;
    while (at && n < max) {                         /* max 가 고리를 끊는다 */
        uint32_t w = pci_read32(h, f->bus, f->dev, f->fn, at);
        ids[n++] = (uint8_t)w;
        at = w >> 8 & 0xFC;
    }
    return n;
}
// [/excerpt]
