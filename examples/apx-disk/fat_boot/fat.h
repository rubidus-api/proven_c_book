/* FAT 파티션의 뿌리 디렉터리에서 파일 하나를 찾아 읽는다.
   부트로더가 커널을 찾는 일이 이것이다. 섹터는 vblk_read 로만 읽는다. */
#ifndef FAT_H
#define FAT_H
#include <stdint.h>
#include "virtio_blk.h"

struct fat {
    struct vblk *disk;
    unsigned bits;           /* 16 또는 32: 클러스터 수로 정해진다 */
    uint32_t fat_lba;        /* 첫 FAT 의 첫 섹터 */
    uint32_t root_lba;       /* FAT16: 뿌리 디렉터리 영역의 첫 섹터 */
    uint32_t root_sectors;   /* FAT16: 그 영역의 크기. FAT32 에서는 0 */
    uint32_t root_cluster;   /* FAT32: 뿌리 디렉터리의 첫 클러스터 */
    uint32_t data_lba;       /* 2번 클러스터의 첫 섹터 */
    uint32_t clusters;
    uint8_t  spc;            /* 클러스터 하나의 섹터 수 */
};

int  fat_mount(struct fat *fs, struct vblk *d, uint32_t part_lba);
long fat_load(struct fat *fs, const char name83[11], uint8_t *dst, uint32_t max);

#endif
