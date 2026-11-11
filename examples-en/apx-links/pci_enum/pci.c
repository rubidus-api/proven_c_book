/* PCI enumeration. What firmware (U-Boot, EDK2) and operating systems do at boot, cut
   down. Hardware is touched only through hw_read32 and hw_write32. */
#include "pci.h"
#include "hw.h"

// [excerpt ecam]
/* ECAM: put the bus, device, function and register numbers in their bit positions and
   that is the address. 4 KiB per function, eight functions per device, thirty-two
   devices per bus. */
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
/* A BAR's size cannot be asked for. Write all ones and read back the low bits the
   device holds at zero. A 64-bit BAR's next slot holds the upper 32 bits. */
static unsigned size_bar(struct pci_host *h, struct pci_fn *f, unsigned i)
{
    unsigned off = BAR0 + 4 * i;
    uint32_t orig = pci_read32(h, f->bus, f->dev, f->fn, off);
    if (orig & 1) return 1;                         /* skip I/O-space BARs */
    int is64 = (orig >> 1 & 3) == 2;

    pci_write32(h, f->bus, f->dev, f->fn, off, 0xFFFFFFFFu);
    uint64_t mask = pci_read32(h, f->bus, f->dev, f->fn, off) & ~0xFull;
    if (is64) {
        pci_write32(h, f->bus, f->dev, f->fn, off + 4, 0xFFFFFFFFu);
        mask |= (uint64_t)pci_read32(h, f->bus, f->dev, f->fn, off + 4) << 32;
    } else {
        mask |= 0xFFFFFFFF00000000ull;
    }
    if ((uint32_t)mask == 0) {                      /* no bits held: there is no BAR */
        pci_write32(h, f->bus, f->dev, f->fn, off, orig);
        return is64 ? 2 : 1;
    }
    uint64_t size = ~mask + 1;  /* the lowest one among the held bits */

    /* Fourth step: write an address back into the BAR. Being firmware, this code writes
       a new address cut from the window, aligned to the size (an operating system would
       restore the firmware's value). That address becomes the address of the device's
       registers. */
    uint64_t at = (h->mem_next + size - 1) & ~(size - 1);
    if (at + size > h->mem_end) at = 0;  /* if the window runs out, assign nothing */
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
            if ((id & 0xFFFF) == 0xFFFF) {          /* all ones when nobody answers */
                if (fn == 0) break;
                continue;
            }
            struct pci_fn f = { .bus = 0, .dev = dev, .fn = fn,
                                .vendor = (uint16_t)id, .device = (uint16_t)(id >> 16),
                                .class_code = pci_read32(h, 0, dev, fn, CLASS) >> 8 };
            uint32_t hdr = pci_read32(h, 0, dev, fn, HDR) >> 16 & 0xFF;
            uint32_t cmd = pci_read32(h, 0, dev, fn, CMD) & 0xFFFF;
            /* While sizing, the BAR holds a nonsense value. Turn off memory and I/O
               decoding so the device does not answer at that address meanwhile. */
            pci_write32(h, 0, dev, fn, CMD, cmd & ~(uint32_t)(CMD_MEM | CMD_IO));
            if ((hdr & 0x7F) == 0)  /* an ordinary device (not a bridge) */
                for (unsigned i = 0; i < 6; )
                    i += size_bar(h, &f, i);
            pci_write32(h, 0, dev, fn, CMD, cmd | CMD_MEM | CMD_MASTER);
            visit(h, &f);
            found++;
            if (fn == 0 && !(hdr & 0x80)) break;  /* a device with a single function */
        }
    }
    return found;
}
// [/excerpt]

// [excerpt caps]
/* Capability list: if bit 4 of the status register is set, 0x34 holds the offset of the
   first entry. It is a linked list whose entries start [type, 1 byte][next offset, 1
   byte]. */
unsigned pci_caps(const struct pci_host *h, const struct pci_fn *f, uint8_t *ids,
                  unsigned max)
{
    if (!(pci_read32(h, f->bus, f->dev, f->fn, CMD) >> 16 & 0x10)) return 0;
    unsigned n = 0;
    unsigned at = pci_read32(h, f->bus, f->dev, f->fn, 0x34) & 0xFC;
    while (at && n < max) {                         /* max breaks a cycle */
        uint32_t w = pci_read32(h, f->bus, f->dev, f->fn, at);
        ids[n++] = (uint8_t)w;
        at = w >> 8 & 0xFC;
    }
    return n;
}
// [/excerpt]
