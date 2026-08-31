/* GPT 를 필드 단위로 짓고 되읽는다 --- 헤더 92바이트, 항목 128바이트, CRC 둘.
   주의: 디스크를 건드리지 않는다. 기억 속 배열만 채운다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SEC        512u
#define ENTRIES    128u          /* 표준이 권하는 최소 개수 */
#define ENTRY_SZ   128u
#define DISK_SECS  4194304u      /* 2 GiB 짜리 디스크라고 하자 */

static uint32_t crc32(const void *buf, size_t n)
{
    const unsigned char *p = buf;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1));
    }
    return c ^ 0xFFFFFFFFu;
}

static void put32(unsigned char *p, uint32_t v)
{ for (int i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i)); }
static void put64(unsigned char *p, uint64_t v)
{ for (int i = 0; i < 8; i++) p[i] = (unsigned char)(v >> (8 * i)); }
static uint32_t get32(const unsigned char *p)
{ uint32_t v = 0; for (int i = 3; i >= 0; i--) v = v << 8 | p[i]; return v; }
static uint64_t get64(const unsigned char *p)
{ uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | p[i]; return v; }

/* ★ GUID 의 함정: 글로 쓸 때와 디스크에 놓일 때의 바이트 차례가 *다르다*.
   앞의 세 마디(4·2·2바이트)는 작은 끝으로 뒤집혀 저장되고, 뒤의 두 마디(2·6바이트)는
   글자 순서 그대로다. 그래서 16진수 덤프와 문서의 GUID 가 달라 보인다. */
static void guid_parse(unsigned char *out, const char *s)
{
    unsigned b[16]; int n = 0;
    for (const char *p = s; *p && n < 16; ) {
        if (*p == '-') { p++; continue; }
        unsigned hi, lo;
        sscanf(p, "%1x%1x", &hi, &lo);
        b[n++] = hi << 4 | lo; p += 2;
    }
    out[0] = (unsigned char)b[3]; out[1] = (unsigned char)b[2];   /* 첫 마디 뒤집기 */
    out[2] = (unsigned char)b[1]; out[3] = (unsigned char)b[0];
    out[4] = (unsigned char)b[5]; out[5] = (unsigned char)b[4];   /* 둘째 마디 */
    out[6] = (unsigned char)b[7]; out[7] = (unsigned char)b[6];   /* 셋째 마디 */
    for (int i = 8; i < 16; i++) out[i] = (unsigned char)b[i];    /* 나머지는 그대로 */
}
static void guid_text(const unsigned char *g, char *out)
{
    sprintf(out, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            g[3], g[2], g[1], g[0], g[5], g[4], g[7], g[6],
            g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
}
static void guid_bytes(const unsigned char *g, char *out)
{ for (int i = 0; i < 16; i++) sprintf(out + i * 3, "%02x ", g[i]); }

/* 이름은 UTF-16LE 36글자 자리(72바이트)에 들어간다 --- ASCII 만 쓴다면 이렇게 */
static void put_name(unsigned char *p, const char *ascii)
{ for (int i = 0; ascii[i]; i++) { p[i * 2] = (unsigned char)ascii[i]; p[i * 2 + 1] = 0; } }

#define ESP   "C12A7328-F81F-11D2-BA4B-00A0C93EC93B"
#define LINUX "0FC63DAF-8483-4772-8E79-3D69D8477DE4"
#define SWAP  "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F"

int main(void)
{
    static unsigned char ents[ENTRIES * ENTRY_SZ];   /* = 16384바이트 = 32섹터 */
    unsigned char hdr[92] = { 0 };
    char t1[64], t2[64];

    /* ── 항목 셋 ─────────────────────────────────────────────── */
    struct { const char *type, *name; uint64_t first, last; uint64_t attr; } part[] = {
        { ESP,   "EFI System",  2048,   206847,  0 },
        { LINUX, "root",        206848, 3358719, 0 },
        { SWAP,  "swap",        3358720, 4194270, 1ull << 63 },  /* 63: 자동 마운트 금지 */
    };
    for (unsigned i = 0; i < sizeof part / sizeof *part; i++) {
        unsigned char *e = ents + i * ENTRY_SZ;
        guid_parse(e, part[i].type);                       /* 0  : 종류 GUID */
        char uniq[40];
        sprintf(uniq, "12345678-1234-5678-9ABC-DEF01234567%X", i);   /* 파티션마다 달라야 한다 */
        guid_parse(e + 16, uniq);                          /* 16 : 이 파티션의 고유 GUID */
        put64(e + 32, part[i].first);                      /* 32 : 첫 LBA */
        put64(e + 40, part[i].last);                       /* 40 : 마지막 LBA(포함) */
        put64(e + 48, part[i].attr);                       /* 48 : 속성 비트 */
        put_name(e + 56, part[i].name);                    /* 56 : 이름 UTF-16LE 72바이트 */
    }

    /* ── 헤더 ────────────────────────────────────────────────── */
    uint64_t backup_lba = DISK_SECS - 1;
    uint64_t ents_sectors = (ENTRIES * ENTRY_SZ + SEC - 1) / SEC;   /* 32 */
    memcpy(hdr, "EFI PART", 8);            /* 0  : 서명 */
    put32(hdr + 8, 0x00010000u);           /* 8  : 개정 1.0 */
    put32(hdr + 12, 92);                   /* 12 : 헤더 크기 */
    put32(hdr + 16, 0);                    /* 16 : 헤더 CRC --- 계산 전에는 0 */
    put32(hdr + 20, 0);                    /* 20 : 예약 */
    put64(hdr + 24, 1);                    /* 24 : 이 헤더의 LBA */
    put64(hdr + 32, backup_lba);           /* 32 : 짝 헤더의 LBA */
    put64(hdr + 40, 2 + ents_sectors);     /* 40 : 쓸 수 있는 첫 LBA = 34 */
    put64(hdr + 48, backup_lba - ents_sectors - 1); /* 48 : 쓸 수 있는 마지막 LBA */
    guid_parse(hdr + 56, "01234567-89AB-CDEF-0123-456789ABCDEF");  /* 56 : 디스크 GUID */
    put64(hdr + 72, 2);                    /* 72 : 항목 배열의 LBA */
    put32(hdr + 80, ENTRIES);              /* 80 : 항목 개수 */
    put32(hdr + 84, ENTRY_SZ);             /* 84 : 항목 하나의 크기 */
    put32(hdr + 88, crc32(ents, ENTRIES * ENTRY_SZ));   /* 88 : 항목 배열 전체의 CRC */
    put32(hdr + 16, crc32(hdr, 92));       /* 마지막에 헤더 자신의 CRC */

    printf("== the GPT header (LBA 1, 92 bytes) ==\n");
    printf("  signature        : %.8s\n", hdr);
    printf("  revision         : %u.%u\n", get32(hdr + 8) >> 16, get32(hdr + 8) & 0xffff);
    printf("  header size      : %u bytes (the sector is 512; the header uses only 92)\n", get32(hdr + 12));
    printf("  header CRC32     : 0x%08x\n", get32(hdr + 16));
    printf("  this / alternate : LBA %llu / LBA %llu\n",
           (unsigned long long)get64(hdr + 24), (unsigned long long)get64(hdr + 32));
    printf("  usable range     : LBA %llu - %llu\n",
           (unsigned long long)get64(hdr + 40), (unsigned long long)get64(hdr + 48));
    guid_text(hdr + 56, t1);
    printf("  disk GUID        : %s\n", t1);
    printf("  entry array      : from LBA %llu, %u x %u bytes = %u sectors\n",
           (unsigned long long)get64(hdr + 72), get32(hdr + 80), get32(hdr + 84),
           (unsigned)ents_sectors);
    printf("  entry array CRC32: 0x%08x\n\n", get32(hdr + 88));

    printf("== checking it ==\n");
    unsigned char probe[92]; memcpy(probe, hdr, 92); put32(probe + 16, 0);
    printf("  header CRC recomputed (that field zeroed) : 0x%08x -> %s\n",
           crc32(probe, 92), crc32(probe, 92) == get32(hdr + 16) ? "matches" : "does not match");
    printf("  entry array CRC recomputed               : 0x%08x -> %s\n\n",
           crc32(ents, ENTRIES * ENTRY_SZ),
           crc32(ents, ENTRIES * ENTRY_SZ) == get32(hdr + 88) ? "matches" : "does not match");

    printf("== entries (128 bytes each) ==\n");
    for (unsigned i = 0; i < 3; i++) {
        const unsigned char *e = ents + i * ENTRY_SZ;
        guid_text(e, t1); guid_bytes(e, t2);
        uint64_t f = get64(e + 32), l = get64(e + 40), a = get64(e + 48);
        char name[40] = { 0 };
        for (int k = 0; k < 36 && e[56 + k * 2]; k++) name[k] = (char)e[56 + k * 2];
        printf("  [%u] name \"%s\"\n", i + 1, name);
        printf("      type GUID (as text) : %s\n", t1);
        printf("      type GUID (byte order): %s\n", t2);
        printf("      LBA %llu ~ %llu  = %.1f MiB\n", (unsigned long long)f,
               (unsigned long long)l, (l - f + 1) * (double)SEC / (1024 * 1024));
        printf("      attributes 0x%016llx%s\n", (unsigned long long)a,
               a & (1ull << 63) ? "  (bit 63 = do not automount)" : "");
    }

    printf("\n== where the alternate (backup) GPT is ==\n");
    printf("  disk %u sectors = %.1f GiB\n", DISK_SECS, DISK_SECS * (double)SEC / (1 << 30));
    printf("  alternate header: the last sector, LBA %llu\n", (unsigned long long)backup_lba);
    printf("  alternate array : LBA %llu - %llu (%u sectors just before the header)\n",
           (unsigned long long)(backup_lba - ents_sectors),
           (unsigned long long)(backup_lba - 1), (unsigned)ents_sectors);
    printf("  -> damage at the front is repaired from the back, and the back from the front.\n");
    return 0;
}
