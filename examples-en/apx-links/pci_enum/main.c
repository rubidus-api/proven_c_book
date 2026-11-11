/* Scan the PCIe of the QEMU virt machine. The ECAM window and the memory window are the
   values in the pci@30000000 entry of the device tree QEMU provides (reg 0x3000_0000,
   ranges 0x4000_0000 + 1 GiB). */
#include "pci.h"
#include "hw.h"

#define FINISH 0x00100000u

static const char *cap_name(uint8_t id)
{
    switch (id) {
    case 0x05: return "MSI";
    case 0x09: return "vendor";
    case 0x10: return "PCIe";
    case 0x11: return "MSI-X";
    default:   return "?";
    }
}

/* QEMU's teaching device edu (1234:11e8): write n to 0x08 of BAR0 and it computes n!
   into the same place, with bit 0 of 0x20 set while busy. This checks that the assigned
   address really reaches the device. */
static void try_edu(uint64_t bar0)
{
    uint32_t id = hw_read32((uintptr_t)bar0);
    hw_write32((uintptr_t)bar0 + 0x08, 5);
    for (int i = 0; i < 100000 && (hw_read32((uintptr_t)bar0 + 0x20) & 1); i++) { }
    uint32_t fact = hw_read32((uintptr_t)bar0 + 0x08);
    hw_log("   edu id register 0x"); hw_log_hex(id, 8);
    hw_log(", 5! = "); hw_log_dec(fact); hw_log("\n");
}

static void visit(const struct pci_host *h, const struct pci_fn *f)
{
    uint8_t ids[8];
    unsigned n = pci_caps(h, f, ids, 8);            /* finish asking the device first */
    hw_log("00:"); hw_log_hex(f->dev, 2); hw_log(".");  hw_log_dec(f->fn);
    hw_log("  "); hw_log_hex(f->vendor, 4); hw_log(":"); hw_log_hex(f->device, 4);
    hw_log("  class "); hw_log_hex(f->class_code, 6);
    if (n) hw_log("  caps");
    for (unsigned i = 0; i < n; i++) { hw_log(" "); hw_log(cap_name(ids[i])); }
    hw_log("\n");
    for (unsigned i = 0; i < 6; i++)
        if (f->bar_size[i]) {
            hw_log("   BAR"); hw_log_dec(i); hw_log(" at 0x"); hw_log_hex(f->bar[i], 8);
            hw_log(", "); hw_log_dec(f->bar_size[i] >> 10); hw_log(" KiB\n");
        }
    if (f->vendor == 0x1234 && f->device == 0x11e8 && f->bar[0]) try_edu(f->bar[0]);
}

int main(void)
{
    struct pci_host h = { .ecam = 0x30000000u,
                          .mem_next = 0x40000000u, .mem_end = 0x80000000u };
    int n = pci_scan(&h, visit);
    hw_log("functions found: "); hw_log_dec((uint64_t)n); hw_log("\n");
    hw_write32(FINISH, 0x5555u);
    return 0;
}
