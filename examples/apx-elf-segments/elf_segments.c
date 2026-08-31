/* 링커가 만드는 「구역」과 로더가 읽는 「조각」은 다른 목록이다.
   여기서는 로더가 보는 쪽 --- 프로그램 헤더 --- 을 제 실행 파일에서 읽는다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    FILE *f = fopen("/proc/self/exe", "rb");
    if (!f) { perror("open"); return 1; }

    unsigned char eh[64];
    if (fread(eh, 1, sizeof eh, f) != sizeof eh) return 1;
    uint64_t phoff = 0; memcpy(&phoff, eh + 32, 8);
    uint16_t phentsize, phnum;
    memcpy(&phentsize, eh + 54, 2); memcpy(&phnum, eh + 56, 2);

    printf("%u program headers\n\n", phnum);
    printf("%-12s %-8s %10s %10s  %s\n", "kind", "perms", "in file", "in memory", "note");
    for (unsigned i = 0; i < phnum; i++) {
        unsigned char ph[56];
        if (fseek(f, (long)(phoff + (uint64_t)i * phentsize), SEEK_SET) != 0) break;
        if (fread(ph, 1, sizeof ph, f) != sizeof ph) break;
        uint32_t type, flags; uint64_t filesz, memsz;
        memcpy(&type, ph, 4); memcpy(&flags, ph + 4, 4);
        memcpy(&filesz, ph + 32, 8); memcpy(&memsz, ph + 40, 8);

        const char *name;
        const char *memo = "";
        switch (type) {
        case 1: name = "PT_LOAD";    memo = "load this as it is"; break;
        case 2: name = "PT_DYNAMIC"; memo = "the table the dynamic linker reads"; break;
        case 3: name = "PT_INTERP";  memo = "* the name of the program that will load me"; break;
        case 4: name = "PT_NOTE";    memo = "notes such as the build id"; break;
        case 6: name = "PT_PHDR";    memo = "this table itself"; break;
        case 0x6474e551: name = "GNU_STACK"; memo = "whether the stack is executable"; break;
        case 0x6474e552: name = "GNU_RELRO"; memo = "read-only after linking"; break;
        default: name = "other"; break;
        }
        char perm[4] = { flags & 4 ? 'r' : '-', flags & 2 ? 'w' : '-', flags & 1 ? 'x' : '-', 0 };
        printf("%-12s %-8s %10llu %10llu  %s\n", name, perm,
               (unsigned long long)filesz, (unsigned long long)memsz, memo);

        if (type == 3) {   /* PT_INTERP 는 문자열 하나를 담는다 */
            uint64_t off; memcpy(&off, ph + 8, 8);
            char buf[128] = { 0 };
            long save = ftell(f);
            fseek(f, (long)off, SEEK_SET);
            fread(buf, 1, sizeof buf - 1 < filesz ? sizeof buf - 1 : (size_t)filesz, f);
            fseek(f, save, SEEK_SET);
            printf("%-12s %-8s %10s %10s  → \"%s\"\n", "", "", "", "", buf);
        }
    }
    fclose(f);

    printf("\nnote: where a piece is larger in memory than in the file,\n");
    printf("      that difference is .bss --- there is no reason to store zeros.\n");
    return 0;
}
