/* The print backend's reply table. Reading where no device is, PCI returns all ones
   (TRACE_DEFAULT). Device 0 answers like QEMU's edu: one 1 MiB BAR0, one capability
   (MSI), and BAR0's registers give the identification value and 5!. */
#define TRACE_DEFAULT 0xFFFFFFFFu
#define C(off) (0x30000000u + (off))
static const struct { uintptr_t addr; unsigned n; uint32_t values[4]; } replies[] = {
    { C(0x00), 1, { 0x11e81234u } },               /* vendor 1234, device 11e8 */
    { C(0x04), 1, { 0x00100000u } },  /* command 0, status: capability list present */
    { C(0x08), 1, { 0x00ff0010u } },
    { C(0x0C), 1, { 0x00000000u } },               /* header type 0, single function */
    /* 0 at first, 1 MiB after writing all ones */
    { C(0x10), 2, { 0x00000000u, 0xfff00000u } },
    { C(0x14), 1, { 0 } }, { C(0x18), 1, { 0 } }, { C(0x1C), 1, { 0 } },
    { C(0x20), 1, { 0 } }, { C(0x24), 1, { 0 } },  /* no other BARs */
    { C(0x34), 1, { 0x00000040u } },
    { C(0x40), 1, { 0x00000005u } },               /* MSI, no next */
    { 0x40000000u, 1, { 0x010000edu } },           /* edu identification register */
    { 0x40000008u, 1, { 120u } },                  /* 5! */
    { 0x40000020u, 2, { 1u, 0u } },                /* busy once, then done */
};
