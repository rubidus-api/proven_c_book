/* The print backend's reply table. Only the last window (0x1000_8000) answers as a
   virtio block device. The status register returns the last value written, so
   FEATURES_OK reads as accepted. But sector contents are written by the device into
   memory by DMA, which this table cannot imitate. */
#define V(off) (0x10008000u + (off))
static const struct { uintptr_t addr; unsigned n; uint32_t values[4]; } replies[] = {
    { V(0x000), 1, { 0x74726976u } },     /* "virt" */
    { V(0x004), 1, { 2u } },              /* virtio-mmio version 2 */
    { V(0x008), 1, { 2u } },              /* device type 2 = block */
    { V(0x010), 1, { 1u } },              /* feature bit 32 = VERSION_1 */
    { V(0x034), 1, { 256u } },            /* largest ring size */
    { V(0x100), 1, { 131072u } },         /* capacity: 64 MiB */
    { V(0x104), 1, { 0u } },
};
