/* PCI Express 버스를 ECAM(설정 공간을 메모리 주소에 펼친 창)으로 훑는 드라이버. */
#ifndef PCI_H
#define PCI_H
#include <stdint.h>

struct pci_host {
    uintptr_t ecam;          /* 설정 공간 창의 시작 */
    uint64_t  mem_next;      /* 장치에 나눠 줄 메모리 창: 다음 빈자리 */
    uint64_t  mem_end;
};

struct pci_fn {
    unsigned bus, dev, fn;
    uint16_t vendor, device;
    uint32_t class_code;     /* 분류 · 하위 분류 · 프로그래밍 인터페이스 */
    uint64_t bar[6];         /* 나눠 준 주소(0 이면 없음) */
    uint64_t bar_size[6];
};

uint32_t pci_read32(const struct pci_host *h, unsigned bus, unsigned dev, unsigned fn,
                    unsigned off);
void pci_write32(const struct pci_host *h, unsigned bus, unsigned dev, unsigned fn,
                 unsigned off, uint32_t v);

/* 버스 0 을 훑어 찾은 기능마다 BAR 에 주소를 나눠 주고 visit 를 부른다. */
int pci_scan(struct pci_host *h, void (*visit)(const struct pci_host *, const struct pci_fn *));

/* 능력 목록의 종류 번호를 ids 에 차례로 담고 개수를 돌려준다. */
unsigned pci_caps(const struct pci_host *h, const struct pci_fn *f, uint8_t *ids, unsigned max);

#endif
