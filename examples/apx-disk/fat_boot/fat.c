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
    fs->bits = fs->clusters < 4085 ? 12 : fs->clusters < 65525 ? 16 : 32;
    if (fs->bits == 32) fs->root_cluster = le32(sec + 44);
    return 0;
}
// [/excerpt]

// [excerpt chain]
/* FAT 은 「이 클러스터 다음은 몇 번」을 적은 배열이다. 항목 하나가 FAT12 는
   1바이트 반, FAT16 은 2바이트, FAT32 는 4바이트(위 4비트는 쓰지 않는다)다.
   끝 표시는 셋 모두 0x0FFFFFFF 로 맞춰 돌려준다. */
static uint32_t next_cluster(struct fat *fs, uint32_t c)
{
    uint32_t at = fs->bits == 12 ? c + c / 2 : c * (fs->bits / 8);
    if (vblk_read(fs->disk, fs->fat_lba + at / 512, sec) != 0) return 0x0FFFFFFF;
    if (fs->bits == 12) {
        /* 12비트 항목 둘이 3바이트에 들어 있어, 한 항목이 섹터 경계에 걸칠 수 있다. */
        uint32_t v = sec[at % 512];
        uint32_t next = fs->fat_lba + at / 512 + 1;
        if (at % 512 == 511 && vblk_read(fs->disk, next, sec) != 0) return 0x0FFFFFFF;
        v |= (uint32_t)sec[(at + 1) % 512] << 8;
        v = c & 1 ? v >> 4 : v & 0xFFF;                     /* 홀수 번은 위 12비트 */
        return v >= 0xFF8 ? 0x0FFFFFFF : v;
    }
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

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

static int same_name(const char *a, const char *b)       /* 대소문자를 가리지 않는다 */
{
    while (*a && lower(*a) == lower(*b)) { a++; b++; }
    return *a == *b;
}

// [excerpt lfn]
/* 긴 이름(LFN)은 짧은 이름 항목 *앞에* 거꾸로 놓인 조각들이다. 조각마다 UCS-2 글자
   13개와 순번(마지막 조각에 0x40), 그리고 짝이 될 짧은 이름의 체크섬을 담는다.
   조각은 디렉터리 섹터의 경계를 넘을 수 있어 상태를 섹터 밖에 둔다. */
struct lfn { char name[261]; uint8_t sum; int valid; };

static void lfn_piece(struct lfn *l, const uint8_t *e)
{
    static const uint8_t at[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
    unsigned ord = e[0] & 0x1F;
    if (e[0] & 0x40) {                                  /* 마지막 조각이 먼저 나온다 */
        l->valid = ord >= 1 && ord <= 20;
        l->sum = e[13];
        if (l->valid) l->name[ord * 13] = '\0';
    }
    if (!l->valid || e[13] != l->sum || ord < 1 || ord > 20) { l->valid = 0; return; }
    for (unsigned k = 0; k < 13; k++) {
        uint16_t ch = le16(e + at[k]);
        char *p = &l->name[(ord - 1) * 13 + k];
        if (ch == 0x0000) *p = '\0';                   /* 이름의 끝 */
        else if (ch != 0xFFFF) *p = ch < 0x80 ? (char)ch : '?';   /* 0xFFFF 는 채움 */
    }
}

/* 짧은 이름 11바이트의 체크섬: 오른쪽으로 한 비트 돌리고 더하기를 되풀이한다. */
static uint8_t short_sum(const uint8_t *e)
{
    uint8_t sum = 0;
    for (int k = 0; k < 11; k++) sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + e[k]);
    return sum;
}
// [/excerpt]

/* 짧은 이름 "KERNEL  BIN" 을 "KERNEL.BIN" 꼴로 */
static void short_name(const uint8_t *e, char out[13])
{
    int n = 0;
    for (int k = 0; k < 8 && e[k] != ' '; k++) out[n++] = (char)e[k];
    if (e[8] != ' ') out[n++] = '.';
    for (int k = 8; k < 11 && e[k] != ' '; k++) out[n++] = (char)e[k];
    out[n] = '\0';
}

/* 디렉터리 섹터 하나를 읽어 이름을 찾는다. 1 = 찾음, 0 = 다음 섹터로, -1 = 목록 끝 · 오류. */
static int scan(struct fat *fs, uint32_t lba, const char *name, struct lfn *l, struct hit *h)
{
    if (vblk_read(fs->disk, lba, sec) != 0) return -1;
    for (int i = 0; i < 512; i += 32) {
        const uint8_t *e = sec + i;
        if (e[0] == 0x00) return -1;                        /* 여기서 목록이 끝난다 */
        if (e[0] == 0xE5) { l->valid = 0; continue; }       /* 지운 항목 */
        if (e[11] == 0x0F) { lfn_piece(l, e); continue; }   /* 긴 이름 조각 */
        char sn[13];
        short_name(e, sn);
        int hit = same_name(name, sn)
                  || (l->valid && l->sum == short_sum(e) && same_name(name, l->name));
        l->valid = 0;                                       /* 조각은 이 항목에서 끝난다 */
        if (hit && !(e[11] & 0x18)) {                       /* 디렉터리 · 볼륨 이름은 빼고 */
            *h = (struct hit){ (uint32_t)le16(e + 20) << 16 | le16(e + 26), le32(e + 28) };
            return 1;
        }
    }
    return 0;
}

// [excerpt find]
/* 뿌리 디렉터리: FAT12 · FAT16 은 FAT 뒤의 고정된 영역,
   FAT32 는 여느 파일처럼 클러스터 사슬이다. */
static int find(struct fat *fs, const char *name, struct hit *h)
{
    struct lfn l;                   /* = { 0 } 은 memset 을 부른다(libc 가 없다) */
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
    return done == size ? (long)size : -2;              /* 사슬이 크기보다 먼저 끝났다 */
}
