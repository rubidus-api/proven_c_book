/* virtio 블록 장치(virtio-mmio, 판 2)의 드라이버 --- 섹터를 읽기만 한다. */
#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H
#include <stdint.h>

struct vblk {
    uintptr_t base;          /* virtio-mmio 레지스터 창 */
    uint64_t  sectors;       /* 장치가 알려 준 크기(512바이트 섹터 수) */
};

int vblk_init(struct vblk *d);                                  /* 0 이면 준비됨 */
int vblk_read(struct vblk *d, uint64_t lba, void *buf512);      /* 0 이면 읽음 */

#endif
