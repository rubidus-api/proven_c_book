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
    fs->bits = fs->clusters < 4085 ? 12 : fs->clusters < 65525 ? 16 : 32;
    if (fs->bits == 32) fs->root_cluster = le32(sec + 44);
    return 0;
}
// [/excerpt]

// [excerpt chain]
/* The FAT is an array that says "after this cluster comes number ...". An entry is
   a byte and a half on FAT12, 2 bytes on FAT16, 4 bytes on FAT32 (top 4 bits unused).
   The end mark is returned as 0x0FFFFFFF for all three. */
static uint32_t next_cluster(struct fat *fs, uint32_t c)
{
    uint32_t at = fs->bits == 12 ? c + c / 2 : c * (fs->bits / 8);
    if (vblk_read(fs->disk, fs->fat_lba + at / 512, sec) != 0) return 0x0FFFFFFF;
    if (fs->bits == 12) {
        /* Two 12-bit entries share 3 bytes, so one entry can straddle a sector
           boundary. */
        uint32_t v = sec[at % 512];
        uint32_t next = fs->fat_lba + at / 512 + 1;
        if (at % 512 == 511 && vblk_read(fs->disk, next, sec) != 0) return 0x0FFFFFFF;
        v |= (uint32_t)sec[(at + 1) % 512] << 8;
        v = c & 1 ? v >> 4 : v & 0xFFF;  /* odd numbers take the top 12 bits */
        return v >= 0xFF8 ? 0x0FFFFFFF : v;
    }
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

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

static int same_name(const char *a, const char *b)       /* case-insensitive */
{
    while (*a && lower(*a) == lower(*b)) { a++; b++; }
    return *a == *b;
}

// [excerpt lfn]
/* A long name (LFN) is pieces stored in reverse *before* the short-name entry. Each
   piece holds 13 UCS-2 characters, a sequence number (0x40 on the last piece) and the
   checksum of the short name it belongs to. Pieces can cross directory sectors, so the
   state lives outside a sector. */
struct lfn { char name[261]; uint8_t sum; int valid; };

static void lfn_piece(struct lfn *l, const uint8_t *e)
{
    static const uint8_t at[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
    unsigned ord = e[0] & 0x1F;
    if (e[0] & 0x40) {                                  /* the last piece comes first */
        l->valid = ord >= 1 && ord <= 20;
        l->sum = e[13];
        if (l->valid) l->name[ord * 13] = '\0';
    }
    if (!l->valid || e[13] != l->sum || ord < 1 || ord > 20) { l->valid = 0; return; }
    for (unsigned k = 0; k < 13; k++) {
        uint16_t ch = le16(e + at[k]);
        char *p = &l->name[(ord - 1) * 13 + k];
        if (ch == 0x0000) *p = '\0';                   /* end of the name */
        else if (ch != 0xFFFF) *p = ch < 0x80 ? (char)ch : '?';  /* 0xFFFF is padding */
    }
}

/* Checksum of the 11-byte short name: rotate right by one bit and add, repeatedly. */
static uint8_t short_sum(const uint8_t *e)
{
    uint8_t sum = 0;
    for (int k = 0; k < 11; k++) sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + e[k]);
    return sum;
}
// [/excerpt]

/* the short name "KERNEL  BIN" as "KERNEL.BIN" */
static void short_name(const uint8_t *e, char out[13])
{
    int n = 0;
    for (int k = 0; k < 8 && e[k] != ' '; k++) out[n++] = (char)e[k];
    if (e[8] != ' ') out[n++] = '.';
    for (int k = 8; k < 11 && e[k] != ' '; k++) out[n++] = (char)e[k];
    out[n] = '\0';
}

/* Read one directory sector and look for the name. 1 = found, 0 = go on to the next
   sector, -1 = end of the list or an error. */
static int scan(struct fat *fs, uint32_t lba, const char *name, struct lfn *l, struct hit *h)
{
    if (vblk_read(fs->disk, lba, sec) != 0) return -1;
    for (int i = 0; i < 512; i += 32) {
        const uint8_t *e = sec + i;
        if (e[0] == 0x00) return -1;                        /* the list ends here */
        if (e[0] == 0xE5) { l->valid = 0; continue; }       /* deleted entry */
        if (e[11] == 0x0F) { lfn_piece(l, e); continue; }   /* a piece of a long name */
        char sn[13];
        short_name(e, sn);
        int hit = same_name(name, sn)
                  || (l->valid && l->sum == short_sum(e) && same_name(name, l->name));
        l->valid = 0;  /* the pieces end at this entry */
        if (hit && !(e[11] & 0x18)) {  /* not a directory or volume label */
            *h = (struct hit){ (uint32_t)le16(e + 20) << 16 | le16(e + 26), le32(e + 28) };
            return 1;
        }
    }
    return 0;
}

// [excerpt find]
/* The root directory: on FAT12 and FAT16 a fixed area after the FATs,
   on FAT32 a cluster chain like any file. */
static int find(struct fat *fs, const char *name, struct hit *h)
{
    struct lfn l;                   /* = { 0 } would call memset (no libc) */
    l.valid = 0;
    int r = 0;
    if (fs->bits != 32)
        for (uint32_t s = 0; s < fs->root_sectors && r == 0; s++)
            r = scan(fs, fs->root_lba + s, name, &l, h);
    else
        for (uint32_t c = fs->root_cluster; !is_end(c) && r == 0;
             c = next_cluster(fs, c))
            for (uint32_t s = 0; s < fs->spc && r == 0; s++)
                r = scan(fs, cluster_lba(fs, c) + s, name, &l, h);
    return r == 1 ? 0 : -1;
}
// [/excerpt]

long fat_load(struct fat *fs, const char *name, uint8_t *dst, uint32_t max)
{
    struct hit h;
    if (find(fs, name, &h) != 0 || h.size > max) return -1;
    uint32_t c = h.first, size = h.size;
    uint32_t done = 0;
    for (; !is_end(c) && done < size; c = next_cluster(fs, c))
        for (uint32_t s = 0; s < fs->spc && done < size; s++) {
            if (vblk_read(fs->disk, cluster_lba(fs, c) + s, sec) != 0) return -2;
            for (uint32_t k = 0; k < 512 && done < size; k++) dst[done++] = sec[k];
        }
    return done == size ? (long)size : -2;  /* the chain ended before the size */
}
