/* 파티션 표 읽기. 펌웨어(UEFI)와 부트로더가 하는 검사를 그대로 한다 ---
   GPT 는 머리말과 항목 배열에 CRC32 가 붙어 있고, 둘 다 맞아야 믿는다. */
#include "part.h"

static uint8_t sec[512];

static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }

// [excerpt crc]
/* CRC-32(IEEE 802.3, 반사형). 표 없이 한 비트씩 ---
   부트로더는 크기가 속도보다 귀하다. */
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
/* lba 에 있는 GPT 머리말과 그것이 가리키는 항목 배열을 읽는다. */
static int gpt(struct vblk *d, uint64_t lba, struct part *out, int max)
{
    if (vblk_read(d, lba, sec) != 0) return -1;
    static const char sig[8] = { 'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T' };
    for (int i = 0; i < 8; i++) if (sec[i] != (uint8_t)sig[i]) return -1;
    uint32_t hsize = le32(sec + 12);
    if (hsize < 92 || hsize > 512) return -1;
    if (le64(sec + 24) != lba) return -1;           /* 적힌 제 자리가 맞아야 한다 */

    /* 머리말의 CRC 는 *자기 칸을 0 으로 두고* 셈한 값이다. */
    uint32_t want = le32(sec + 16);
    sec[16] = sec[17] = sec[18] = sec[19] = 0;
    if (~crc32_update(~0u, sec, hsize) != want) return -1;

    uint64_t entries_lba = le64(sec + 72);
    uint32_t count = le32(sec + 80), esize = le32(sec + 84);
    uint32_t entries_crc = le32(sec + 88);
    if (esize != 128 || count > 1024) return -1;

    /* 항목 배열 전체의 CRC 를 섹터 단위로 이어 셈하면서 쓰는 항목을 골라 둔다. */
    uint32_t crc = ~0u;
    int n = 0;
    for (uint32_t s = 0; s < (count * esize + 511) / 512; s++) {
        if (vblk_read(d, entries_lba + s, sec) != 0) return -1;
        uint32_t bytes = count * esize - s * 512 < 512 ? count * esize - s * 512 : 512;
        crc = crc32_update(crc, sec, bytes);
        for (uint32_t e = 0; e < bytes; e += 128) {
            const uint8_t *p = sec + e;
            int used = 0;
            for (int k = 0; k < 16; k++) used |= p[k];  /* 종류 GUID 0 = 빈 칸 */
            if (!used || n >= max) continue;
            out[n] = (struct part){ .scheme = 'G', .lba = le64(p + 32) };
            out[n].count = le64(p + 40) - out[n].lba + 1;    /* 끝 LBA 는 포함이다 */
            for (int k = 0; k < 16; k++) out[n].guid[k] = p[k];
            n++;
        }
    }
    return ~crc == entries_crc ? n : -1;
}
// [/excerpt]

// [excerpt mbr]
int partitions(struct vblk *d, struct part *out, int max, uint64_t *gpt_at)
{
    *gpt_at = 0;
    if (vblk_read(d, 0, sec) != 0) return -2;             /* 장치가 대답하지 않았다 */
    if (sec[510] != 0x55 || sec[511] != 0xAA) return -1;  /* 서명이 없으면 MBR 아님 */
    if (sec[446 + 4] == 0xEE) {                           /* 보호 MBR: 진짜는 GPT */
        /* 앞의 GPT(1번 섹터)가 검사에 걸리면 디스크 맨 끝 섹터의 짝 GPT 를 읽는다.
           짝의 항목 배열은 짝 머리말 바로 앞에 있고, 머리말이 그 자리를 적고 있다. */
        *gpt_at = 1;
        int n = gpt(d, *gpt_at, out, max);
        if (n < 0) {
            *gpt_at = d->sectors - 1;
            n = gpt(d, *gpt_at, out, max);
        }
        return n;
    }
    int n = 0;
    for (int i = 0; i < 4 && n < max; i++) {
        const uint8_t *e = sec + 446 + 16 * i;               /* 항목 16바이트 넷 */
        if (e[4] == 0) continue;                             /* 종류 0 = 빈 항목 */
        out[n++] = (struct part){ .scheme = 'M', .mbr_type = e[4],
                                  .lba = le32(e + 8), .count = le32(e + 12) };
    }
    return n;
}
// [/excerpt]
