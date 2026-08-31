/* 파일 앞머리 몇 바이트로 「이것이 무슨 형식인가」를 가른다.
   형식이 여럿이어도 묻는 질문은 같다: 매직, 폭, 종류, 진입점.
   인자로 준 파일들을 차례로 본다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int rd(FILE *f, long off, void *p, size_t n)
{ return fseek(f, off, SEEK_SET) == 0 && fread(p, 1, n, f) == n; }

static uint16_t u16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void show_elf(FILE *f)
{
    unsigned char h[64];
    if (!rd(f, 0, h, sizeof h)) return;
    static const char *types[] = { "ET_NONE", "ET_REL (object file)", "ET_EXEC (fixed address)",
                                   "ET_DYN (shared object / PIE)", "ET_CORE (core dump)" };
    unsigned t = u16(h + 16);
    printf("  format   : ELF\n");
    printf("  width    : %s\n", h[4] == 2 ? "64-bit" : "32-bit");
    printf("  byte order: %s\n", h[5] == 1 ? "little-endian" : "big-endian");
    printf("  kind     : %s\n", t < 5 ? types[t] : "other");
    printf("  entry    : 0x%llx\n", (unsigned long long)
           (h[4] == 2 ? (uint64_t)u32(h + 24) | (uint64_t)u32(h + 28) << 32 : u32(h + 24)));
}

static void show_pe(FILE *f)
{
    unsigned char mz[64], pe[24], opt[2];
    if (!rd(f, 0, mz, sizeof mz)) return;
    long e_lfanew = u32(mz + 60);            /* MZ 헤더가 가리키는 「진짜 헤더」의 자리 */
    printf("  format   : MZ (DOS executable header)\n");
    printf("  next hdr : file offset 0x%lx\n", e_lfanew);
    if (!rd(f, e_lfanew, pe, sizeof pe)) return;
    if (memcmp(pe, "PE\0\0", 4) != 0) { printf("  no PE signature there --- a pure DOS executable\n"); return; }
    printf("  format   : PE (so this file is MZ + PE, two layers)\n");
    printf("  machine  : 0x%04x%s\n", u16(pe + 4), u16(pe + 4) == 0x8664 ? " (x86-64)" : "");
    printf("  sections : %u\n", u16(pe + 6));
    if (rd(f, e_lfanew + 24, opt, 2))
        printf("  width    : %s\n", u16(opt) == 0x20b ? "PE32+ (64-bit)"
                                  : u16(opt) == 0x10b ? "PE32 (32-bit)" : "other");
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) { printf("%s: cannot open\n", argv[i]); continue; }
        unsigned char m[4] = { 0 };
        rd(f, 0, m, 4);
        printf("%s\n", argv[i]);
        printf("  magic    : %02x %02x %02x %02x\n", m[0], m[1], m[2], m[3]);
        if (!memcmp(m, "\x7f" "ELF", 4))      show_elf(f);
        else if (!memcmp(m, "MZ", 2))         show_pe(f);
        else if (u32(m) == 0xfeedfacfu)       printf("  format   : Mach-O (64-bit)\n");
        else if (u32(m) == 0xcafebabeu)       printf("  format   : Java class, or a Mach-O universal binary --- the magic collides\n");
        else if (!memcmp(m, "\0asm", 4))      printf("  format   : WebAssembly\n");
        else if (u16(m) == 0407 || u16(m) == 0410 || u16(m) == 0413)
            printf("  format   : a.out (magic 0%o --- old Unix)\n", u16(m));
        else printf("  format   : unknown --- possibly a format with no magic (a DOS .COM, say)\n");
        fclose(f);
    }
    return 0;
}
