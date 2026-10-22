/* Build a GPT field by field and read it back --- a 92-byte header, 128-byte entries, two CRCs.
   Note: no disk is touched. Only an array in memory is filled. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SEC        512u
#define ENTRIES    128u          /* the minimum count the standard recommends */
#define ENTRY_SZ   128u
#define DISK_SECS  4194304u      /* say it is a 2 GiB disk */

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

/* ★ The GUID trap: the byte order in writing and on disk *differ*.
   The first three groups (4, 2, 2 bytes) are stored reversed, little-endian; the last two
   (2, 6 bytes) keep text order. So a hex dump and a documented GUID look different. */
static void guid_parse(unsigned char *out, const char *s)
{
    unsigned b[16]; int n = 0;
    for (const char *p = s; *p && n < 16; ) {
        if (*p == '-') { p++; continue; }
        unsigned hi, lo;
        sscanf(p, "%1x%1x", &hi, &lo);
        b[n++] = hi << 4 | lo; p += 2;
    }
    out[0] = (unsigned char)b[3]; out[1] = (unsigned char)b[2];   /* reverse the first group */
    out[2] = (unsigned char)b[1]; out[3] = (unsigned char)b[0];
    out[4] = (unsigned char)b[5]; out[5] = (unsigned char)b[4];   /* the second group */
    out[6] = (unsigned char)b[7]; out[7] = (unsigned char)b[6];   /* the third group */
    for (int i = 8; i < 16; i++) out[i] = (unsigned char)b[i];    /* the rest as is */
}
static void guid_text(const unsigned char *g, char *out)
{
    sprintf(out, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            g[3], g[2], g[1], g[0], g[5], g[4], g[7], g[6],
            g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
}
static void guid_bytes(const unsigned char *g, char *out)
{ for (int i = 0; i < 16; i++) sprintf(out + i * 3, "%02x ", g[i]); }

/* The name goes in 36 UTF-16LE characters (72 bytes) --- if it is ASCII only, like this */
static void put_name(unsigned char *p, const char *ascii)
{ for (int i = 0; ascii[i]; i++) { p[i * 2] = (unsigned char)ascii[i]; p[i * 2 + 1] = 0; } }

#define ESP   "C12A7328-F81F-11D2-BA4B-00A0C93EC93B"
#define LINUX "0FC63DAF-8483-4772-8E79-3D69D8477DE4"
#define SWAP  "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F"

int main(void)
{
    static unsigned char ents[ENTRIES * ENTRY_SZ];   /* = 16384 bytes = 32 sectors */
    unsigned char hdr[92] = { 0 };
    char t1[64], t2[64];

    /* ── three entries ───────────────────────────────────────── */
    struct { const char *type, *name; uint64_t first, last; uint64_t attr; } part[] = {
        { ESP,   "EFI System",  2048,   206847,  0 },
        { LINUX, "root",        206848, 3358719, 0 },
        { SWAP,  "swap",        3358720, 4194270, 1ull << 63 },  /* 63: do not auto-mount */
    };
    for (unsigned i = 0; i < sizeof part / sizeof *part; i++) {
        unsigned char *e = ents + i * ENTRY_SZ;
        guid_parse(e, part[i].type);                       /* 0  : type GUID */
        char uniq[40];
        sprintf(uniq, "12345678-1234-5678-9ABC-DEF01234567%X", i);   /* must differ per partition */
        guid_parse(e + 16, uniq);                          /* 16 : this partition's unique GUID */
        put64(e + 32, part[i].first);                      /* 32 : first LBA */
        put64(e + 40, part[i].last);                       /* 40 : last LBA (inclusive) */
        put64(e + 48, part[i].attr);                       /* 48 : attribute bits */
        put_name(e + 56, part[i].name);                    /* 56 : name, UTF-16LE, 72 bytes */
    }

    /* ── header ──────────────────────────────────────────────── */
    uint64_t backup_lba = DISK_SECS - 1;
    uint64_t ents_sectors = (ENTRIES * ENTRY_SZ + SEC - 1) / SEC;   /* 32 */
    memcpy(hdr, "EFI PART", 8);            /* 0  : signature */
    put32(hdr + 8, 0x00010000u);           /* 8  : revision 1.0 */
    put32(hdr + 12, 92);                   /* 12 : header size */
    put32(hdr + 16, 0);                    /* 16 : header CRC --- 0 before computing */
    put32(hdr + 20, 0);                    /* 20 : reserved */
    put64(hdr + 24, 1);                    /* 24 : LBA of this header */
    put64(hdr + 32, backup_lba);           /* 32 : LBA of the partner header */
    put64(hdr + 40, 2 + ents_sectors);     /* 40 : first usable LBA = 34 */
    put64(hdr + 48, backup_lba - ents_sectors - 1); /* 48 : last usable LBA */
    guid_parse(hdr + 56, "01234567-89AB-CDEF-0123-456789ABCDEF");  /* 56 : disk GUID */
    put64(hdr + 72, 2);                    /* 72 : LBA of the entry array */
    put32(hdr + 80, ENTRIES);              /* 80 : number of entries */
    put32(hdr + 84, ENTRY_SZ);             /* 84 : size of one entry */
    put32(hdr + 88, crc32(ents, ENTRIES * ENTRY_SZ));   /* 88 : CRC of the whole entry array */
    put32(hdr + 16, crc32(hdr, 92));       /* last, the header's own CRC */

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
