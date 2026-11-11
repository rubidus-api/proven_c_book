/* Read a disk's partition table: the MBR, and GPT when the MBR says it is "protective".
*/
#ifndef PART_H
#define PART_H
#include <stdint.h>
#include "virtio_blk.h"

struct part {
    char     scheme;         /* 'M' = MBR entry, 'G' = GPT entry */
    uint8_t  mbr_type;       /* the MBR type byte */
    uint8_t  guid[16];       /* the GPT type GUID (bytes as stored on the disk) */
    uint64_t lba, count;
};

/* Number of partitions found. -1 if there is no table or a check fails, -2 if sector 0
   could not be read. */
int partitions(struct vblk *d, struct part *out, int max);

#endif
