/* Driver for a virtio block device (virtio-mmio, version 2) --- it only reads sectors.
*/
#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H
#include <stdint.h>

struct vblk {
    uintptr_t base;          /* the virtio-mmio register window */
    uint64_t  sectors;       /* the size the device reported (512-byte sectors) */
};

int vblk_init(struct vblk *d);                                  /* 0 when ready */
int vblk_read(struct vblk *d, uint64_t lba, void *buf512);      /* 0 when read */

#endif
