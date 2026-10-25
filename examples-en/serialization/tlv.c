/* A small file format: magic, version, then tag-length-value fields.
   A reader built for version 1 meets a file written by version 2 and
   skips the field it has never heard of --- because every field says how long it is. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { TAG_NAME = 1, TAG_SCORE = 2, TAG_COLOUR = 3 /* added in version 2 */ };
enum { MAX_FIELD = 64 };                    /* the format promises: no field is longer */

static const unsigned char magic[4] = { 'P', 'C', 'B', 0x1A };

static size_t put_field(unsigned char *out, uint8_t tag, const void *v, uint8_t len)
{
    out[0] = tag; out[1] = len;
    memcpy(out + 2, v, len);
    return 2u + len;
}

static size_t write_v2(unsigned char *out)
{
    size_t n = 0;
    memcpy(out, magic, 4); n += 4;
    out[n++] = 2;                                         /* version */
    n += put_field(out + n, TAG_NAME, "alice", 5);
    n += put_field(out + n, TAG_COLOUR, "\xFF\x80\x00", 3);   /* new in v2 */
    unsigned char score[4] = { 0x39, 0x05, 0, 0 };        /* 1337, little endian */
    n += put_field(out + n, TAG_SCORE, score, 4);
    return n;
}

/* The version-1 reader: knows NAME and SCORE only. */
static int read_v1(const unsigned char *in, size_t len)
{
    if (len < 5 || memcmp(in, magic, 4) != 0) { puts("  not our file"); return 0; }
    unsigned version = in[4];
    printf("  version %u file\n", version);
    size_t pos = 5;
    while (pos < len) {
        if (len - pos < 2) { puts("  rejected: field header cut short"); return 0; }
        unsigned tag = in[pos], flen = in[pos + 1];
        if (flen > MAX_FIELD)
            { printf("  rejected: field of %u bytes\n", flen); return 0; }
        if (flen > len - pos - 2)
            { puts("  rejected: field runs past the end"); return 0; }
        const unsigned char *v = in + pos + 2;
        switch (tag) {
        case TAG_NAME:
            printf("  name  = %.*s\n", (int)flen, (const char *)v);
            break;
        case TAG_SCORE:
            if (flen != 4) { puts("  rejected: score is not 4 bytes"); return 0; }
            printf("  score = %u\n", (unsigned)(v[0] | v[1] << 8
                                     | (uint32_t)v[2] << 16 | (uint32_t)v[3] << 24));
            break;
        default:
            printf("  (skipped unknown tag %u, %u bytes)\n", tag, flen);
        }
        pos += 2u + flen;
    }
    return 1;
}

int main(void)
{
    unsigned char file[128];
    size_t n = write_v2(file);
    printf("[a version-2 file, %zu bytes, read by a version-1 reader]\n", n);
    read_v1(file, n);

    puts("\n[the same file with the last field's length raised to 9]");
    file[n - 5] = 9;
    read_v1(file, n);

    puts("\n[a field that claims 200 bytes]");
    file[6] = 200;
    read_v1(file, n);
    return 0;
}
