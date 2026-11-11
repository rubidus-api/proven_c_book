/* 디스크의 파티션 표를 읽는다: MBR, 그리고 MBR 이 「보호용」이라고 말하면 GPT. */
#ifndef PART_H
#define PART_H
#include <stdint.h>
#include "virtio_blk.h"

struct part {
    char     scheme;         /* 'M' = MBR 항목, 'G' = GPT 항목 */
    uint8_t  mbr_type;       /* MBR 의 종류 바이트 */
    uint8_t  guid[16];       /* GPT 의 종류 GUID(디스크에 놓인 바이트 그대로) */
    uint64_t lba, count;
};

/* 찾은 파티션 수. 표가 없거나 검사가 어긋나면 -1, 0번 섹터를 못 읽었으면 -2. */
int partitions(struct vblk *d, struct part *out, int max);

#endif
