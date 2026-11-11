/* Driver that scans a PCI Express bus through ECAM (configuration space laid out in
   memory). */
#ifndef PCI_H
#define PCI_H
#include <stdint.h>

struct pci_host {
    uintptr_t ecam;          /* start of the configuration space window */
    uint64_t  mem_next;      /* memory window to hand out to devices: next free place */
    uint64_t  mem_end;
};

struct pci_fn {
    unsigned bus, dev, fn;
    uint16_t vendor, device;
    uint32_t class_code;     /* class, subclass, programming interface */
    uint64_t bar[6];         /* assigned address (0 = none) */
    uint64_t bar_size[6];
};

uint32_t pci_read32(const struct pci_host *h, unsigned bus, unsigned dev, unsigned fn,
                    unsigned off);
void pci_write32(const struct pci_host *h, unsigned bus, unsigned dev, unsigned fn,
                 unsigned off, uint32_t v);

/* Scan bus 0, give each function found addresses for its BARs, and call visit. */
int pci_scan(struct pci_host *h, void (*visit)(const struct pci_host *, const struct pci_fn *));

/* Store the capability type numbers in ids in order and return how many. */
unsigned pci_caps(const struct pci_host *h, const struct pci_fn *f, uint8_t *ids, unsigned max);

#endif
