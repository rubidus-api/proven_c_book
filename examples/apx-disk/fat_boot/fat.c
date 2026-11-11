/* FAT 을 읽는 부트로더의 한가운데. 디스크의 바이트는 모두 작은 끝(little-endian)
   이고 정렬되어 있지 않으므로, 구조체를 덮어씌우지 않고 바이트로 읽어 맞춘다. */
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
    uint16_t rootents = le16(sec + 17);                     /* FAT32 에서는 0 */
    uint32_t fatsz    = le16(sec + 22) ? le16(sec + 22) : le32(sec + 36);
    uint32_t total    = le16(sec + 19) ? le16(sec + 19) : le32(sec + 32);
    uint32_t rootsecs = (rootents * 32u + 511) / 512;

    *fs = (struct fat){ .disk = d, .spc = sec[13], .root_sectors = rootsecs };
    fs->fat_lba  = part_lba + reserved;
    fs->root_lba = fs->fat_lba + nfats * fatsz;             /* FAT 들 바로 뒤 */
    fs->data_lba = fs->root_lba + rootsecs;                 /* FAT32 에서는 같은 자리 */
    fs->clusters = (total - (reserved + nfats * fatsz + rootsecs)) / fs->spc;

    /* FAT 의 종류는 이름표("FAT16   ")가 아니라 클러스터 수가 정한다(명세의 규칙). */
    if (fs->clusters < 4085) return -1;                     /* FAT12: 다루지 않는다 */
    fs->bits = fs->clusters < 65525 ? 16 : 32;
    if (fs->bits == 32) fs->root_cluster = le32(sec + 44);
    return 0;
}
// [/excerpt]

// [excerpt chain]
/* FAT 은 「이 클러스터 다음은 몇 번」을 적은 배열이다. FAT16 은 항목 2바이트,
   FAT32 는 4바이트이고 위 4비트는 쓰지 않는다. */
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
{ return fs->data_lba + (c - 2) * fs->spc; }                /* 클러스터 번호는 2 부터 */
// [/excerpt]

static int is_end(uint32_t c) { return c < 2 || c >= 0x0FFFFFF8; }

struct hit { uint32_t first, size; };

/* 디렉터리 섹터 하나를 읽어 이름을 찾는다. 1 = 찾음, 0 = 다음 섹터로, -1 = 목록 끝 · 오류. */
static int scan(struct fat *fs, uint32_t lba, const char name83[11], struct hit *h)
{
    if (vblk_read(fs->disk, lba, sec) != 0) return -1;
    for (int i = 0; i < 512; i += 32) {
        const uint8_t *e = sec + i;
        if (e[0] == 0x00) return -1;                        /* 여기서 목록이 끝난다 */
        if (e[0] == 0xE5 || e[11] == 0x0F) continue;        /* 지운 항목 · 긴 이름 조각 */
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
/* 뿌리 디렉터리: FAT16 은 FAT 뒤의 고정된 영역,
   FAT32 는 여느 파일처럼 클러스터 사슬이다. */
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
