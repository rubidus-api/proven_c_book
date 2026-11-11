/* Find one file in the root directory of a FAT partition and read it. This is how a
   bootloader finds the kernel. Sectors are read only through vblk_read. */
#ifndef FAT_H
#define FAT_H
#include <stdint.h>
#include "virtio_blk.h"

struct fat {
    struct vblk *disk;
    unsigned bits;           /* 16 or 32: decided by the cluster count */
    uint32_t fat_lba;        /* first sector of the first FAT */
    uint32_t root_lba;       /* FAT16: first sector of the root directory area */
    uint32_t root_sectors;   /* FAT16: the size of that area. 0 on FAT32 */
    uint32_t root_cluster;   /* FAT32: first cluster of the root directory */
    uint32_t data_lba;       /* first sector of cluster 2 */
    uint32_t clusters;
    uint8_t  spc;            /* sectors in one cluster */
};

int  fat_mount(struct fat *fs, struct vblk *d, uint32_t part_lba);
long fat_load(struct fat *fs, const char name83[11], uint8_t *dst, uint32_t max);

#endif
