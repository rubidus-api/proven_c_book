/* The heart of a bootloader that reads FAT. Every byte on the disk is little-endian and
   unaligned, so instead of overlaying a struct, the fields are assembled from bytes. */
#include "fat.h"

static uint8_t sec[512];

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

// [excerpt mount]
int fat_mount(struct fat *fs, struct vblk *d, uint32_t part_lba)
{
    if (vblk_read(d, part_lba, sec) != 0) return -1;
    if (le16(sec + 11) != 512 || sec[13] == 0 || sec[510] != 0x55 || sec[511] != 0xAA)
        return -1;
    uint16_t reserved = le16(sec + 14);
    uint8_t  nfats    = sec[16];
    uint16_t rootents = le16(sec + 17);                     /* 0 on FAT32 */
    uint32_t fatsz    = le16(sec + 22) ? le16(sec + 22) : le32(sec + 36);
    uint32_t total    = le16(sec + 19) ? le16(sec + 19) : le32(sec + 32);
    uint32_t rootsecs = (rootents * 32u + 511) / 512;

    *fs = (struct fat){ .disk = d, .spc = sec[13], .root_sectors = rootsecs };
    fs->fat_lba  = part_lba + reserved;
    fs->root_lba = fs->fat_lba + nfats * fatsz;             /* right after the FATs */
    fs->data_lba = fs->root_lba + rootsecs;  /* the same place on FAT32 */
    fs->clusters = (total - (reserved + nfats * fatsz + rootsecs)) / fs->spc;

    /* The FAT type is decided by the cluster count, not by the label ("FAT16   ") (the
       specification's rule). */
    if (fs->clusters < 4085) return -1;                     /* FAT12: not handled */
    fs->bits = fs->clusters < 65525 ? 16 : 32;
    if (fs->bits == 32) fs->root_cluster = le32(sec + 44);
    return 0;
}
// [/excerpt]

// [excerpt chain]
/* The FAT is an array that says "after this cluster comes number ...". On FAT16 an
   entry is 2 bytes, on FAT32 4 bytes with the top 4 bits unused. */
static uint32_t next_cluster(struct fat *fs, uint32_t c)
{
    uint32_t at = c * (fs->bits / 8);
    if (vblk_read(fs->disk, fs->fat_lba + at / 512, sec) != 0) return 0x0FFFFFFF;
    if (fs->bits == 16) {
        uint16_t v = le16(sec + at % 512);
        return v >= 0xFFF8 ? 0x0FFFFFFF : v;
    }
    return le32(sec + at % 512) & 0x0FFFFFFF;
}

static uint32_t cluster_lba(const struct fat *fs, uint32_t c)
{ return fs->data_lba + (c - 2) * fs->spc; }  /* cluster numbers start at 2 */
// [/excerpt]

static int is_end(uint32_t c) { return c < 2 || c >= 0x0FFFFFF8; }

struct hit { uint32_t first, size; };

/* Read one directory sector and look for the name. 1 = found, 0 = go on to the next
   sector, -1 = end of the list or an error. */
static int scan(struct fat *fs, uint32_t lba, const char name83[11], struct hit *h)
{
    if (vblk_read(fs->disk, lba, sec) != 0) return -1;
    for (int i = 0; i < 512; i += 32) {
        const uint8_t *e = sec + i;
        if (e[0] == 0x00) return -1;                        /* the list ends here */
        /* deleted entry, or a piece of a long name */
        if (e[0] == 0xE5 || e[11] == 0x0F) continue;
        int same = 1;
        for (int k = 0; k < 11; k++) same &= e[k] == (uint8_t)name83[k];
        if (same) {
            *h = (struct hit){ (uint32_t)le16(e + 20) << 16 | le16(e + 26), le32(e + 28) };
            return 1;
        }
    }
    return 0;
}

// [excerpt find]
/* The root directory: on FAT16 a fixed area after the FATs,
   on FAT32 a cluster chain like any file. */
static int find(struct fat *fs, const char name83[11], struct hit *h)
{
    int r = 0;
    if (fs->bits == 16)
        for (uint32_t s = 0; s < fs->root_sectors && r == 0; s++)
            r = scan(fs, fs->root_lba + s, name83, h);
    else
        for (uint32_t c = fs->root_cluster; !is_end(c) && r == 0;
             c = next_cluster(fs, c))
            for (uint32_t s = 0; s < fs->spc && r == 0; s++)
                r = scan(fs, cluster_lba(fs, c) + s, name83, h);
    return r == 1 ? 0 : -1;
}
// [/excerpt]

long fat_load(struct fat *fs, const char name83[11], uint8_t *dst, uint32_t max)
{
    struct hit h;
    if (find(fs, name83, &h) != 0 || h.size > max) return -1;
    uint32_t c = h.first, size = h.size;
    uint32_t done = 0;
    for (; !is_end(c) && done < size; c = next_cluster(fs, c))
        for (uint32_t s = 0; s < fs->spc && done < size; s++) {
            if (vblk_read(fs->disk, cluster_lba(fs, c) + s, sec) != 0) return -1;
            for (uint32_t k = 0; k < 512 && done < size; k++) dst[done++] = sec[k];
        }
    return done == size ? (long)size : -1;
}
