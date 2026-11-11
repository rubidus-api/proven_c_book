/* Reading the partition table, with the checks firmware (UEFI) and bootloaders make ---
   GPT puts a CRC32 on the header and on the entry array, and both must match to be
   trusted. */
#include "part.h"

static uint8_t sec[512];

static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }

// [excerpt crc]
/* CRC-32 (IEEE 802.3, reflected). One bit at a time, no table ---
   in a bootloader size is worth more than speed. */
static uint32_t crc32_update(uint32_t crc, const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) crc = crc >> 1 ^ (0xEDB88320u & -(crc & 1));
    }
    return crc;
}
// [/excerpt]

// [excerpt gpt]
static int gpt(struct vblk *d, struct part *out, int max)
{
    if (vblk_read(d, 1, sec) != 0) return -1;
    static const char sig[8] = { 'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T' };
    for (int i = 0; i < 8; i++) if (sec[i] != (uint8_t)sig[i]) return -1;
    uint32_t hsize = le32(sec + 12);
    if (hsize < 92 || hsize > 512) return -1;

    /* The header CRC is computed *with its own field set to zero*. */
    uint32_t want = le32(sec + 16);
    sec[16] = sec[17] = sec[18] = sec[19] = 0;
    if (~crc32_update(~0u, sec, hsize) != want) return -1;

    uint64_t entries_lba = le64(sec + 72);
    uint32_t count = le32(sec + 80), esize = le32(sec + 84);
    uint32_t entries_crc = le32(sec + 88);
    if (esize != 128 || count > 1024) return -1;

    /* Compute the CRC of the whole entry array sector by sector, picking out the used
       entries on the way. */
    uint32_t crc = ~0u;
    int n = 0;
    for (uint32_t s = 0; s < (count * esize + 511) / 512; s++) {
        if (vblk_read(d, entries_lba + s, sec) != 0) return -1;
        uint32_t bytes = count * esize - s * 512 < 512 ? count * esize - s * 512 : 512;
        crc = crc32_update(crc, sec, bytes);
        for (uint32_t e = 0; e < bytes; e += 128) {
            const uint8_t *p = sec + e;
            int used = 0;
            for (int k = 0; k < 16; k++) used |= p[k];  /* zero type GUID = empty */
            if (!used || n >= max) continue;
            out[n] = (struct part){ .scheme = 'G', .lba = le64(p + 32) };
            /* the ending LBA is inclusive */
            out[n].count = le64(p + 40) - out[n].lba + 1;
            for (int k = 0; k < 16; k++) out[n].guid[k] = p[k];
            n++;
        }
    }
    return ~crc == entries_crc ? n : -1;
}
// [/excerpt]

// [excerpt mbr]
int partitions(struct vblk *d, struct part *out, int max)
{
    if (vblk_read(d, 0, sec) != 0) return -2;  /* the device did not answer */
    if (sec[510] != 0x55 || sec[511] != 0xAA) return -1;  /* no signature, no MBR */
    if (sec[446 + 4] == 0xEE) return gpt(d, out, max);    /* protective MBR: GPT */
    int n = 0;
    for (int i = 0; i < 4 && n < max; i++) {
        const uint8_t *e = sec + 446 + 16 * i;  /* four entries of 16 bytes */
        if (e[4] == 0) continue;                             /* type 0 = empty entry */
        out[n++] = (struct part){ .scheme = 'M', .mbr_type = e[4],
                                  .lba = le32(e + 8), .count = le32(e + 12) };
    }
    return n;
}
// [/excerpt]
