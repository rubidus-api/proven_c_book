/* 출력 백엔드: 이 기계에서 BIOS 노릇을 한다. 디스크는 run.sh 가 만든 이미지 파일이고,
   부트 코드가 내리는 명령(디스크 읽기, 뛰기, 끝내기)을 한 줄씩 찍는다.
   직렬 포트로 보내는 글자는 그대로 내보낸다. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bios.h"

void mbr_main(uint8_t drive, const struct part *table);
[[noreturn]] void vbr_main(uint8_t drive, const struct part *entry);

static FILE *disk;
static uint8_t low[0x200];           /* 0x0600 --- MBR 이 자기를 옮겨 둔 자리 */
static uint8_t at7c00[0x200];        /* 0x7c00 --- BIOS 와 MBR 이 다음 단계를 싣는 자리 */

void con_putc(char c) { if (c != '\r') putchar(c); }

int disk_read(uint8_t drive, uint32_t lba, void *dst)
{
    uint16_t to = real_addr(dst);
    if (to == 0xffff) printf("int 13h AH=42h  DL=%02x  LBA %u -> a VBR buffer\n", drive, lba);
    else              printf("int 13h AH=42h  DL=%02x  LBA %u -> %04x\n", drive, lba, to);
    if (fseek(disk, (long)lba * 512, SEEK_SET) != 0) return 1;
    return fread(dst, 512, 1, disk) == 1 ? 0 : 1;
}

uint8_t *load_area(void) { return at7c00; }

uint16_t real_addr(const void *p)
{
    const uint8_t *b = p;
    if (b >= low && b < low + sizeof low) return (uint16_t)(0x0600 + (b - low));
    if (b >= at7c00 && b < at7c00 + sizeof at7c00) return (uint16_t)(0x7c00 + (b - at7c00));
    return 0xffff;                   /* 그 밖은 VBR 의 작업 버퍼 --- 실모드 주소를 따지지 않는다 */
}

/* 진짜 기계에서 「제어를 넘긴다」는 그 주소로 뛰는 것이다. 여기서는 다음 단계의 C 함수를
   부르는 것으로 대신한다 --- VBR 의 첫 섹터가 하는 나머지 섹터 싣기(vboot.c)는 건너뛴다. */
void chain(uint8_t drive, const struct part *entry, void *code)
{
    printf("jmp 0000:%04x  DL=%02x  SI=%04x\n", real_addr(code), drive, real_addr(entry));
    vbr_main(drive, entry);
}

void stop(uint8_t code)
{
    printf("out 0xf4 <- %02x  (stop)\n", code);
    fclose(disk);
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(disk = fopen(argv[1], "rb"))) { fprintf(stderr, "usage: trace disk.img\n"); return 1; }
    /* BIOS 가 하는 일: 0번 섹터를 0x7c00 에 싣고, 끝 두 바이트를 보고, DL=80 으로 뛴다 */
    if (fread(at7c00, 512, 1, disk) != 1 || at7c00[510] != 0x55 || at7c00[511] != 0xaa) {
        puts("BIOS: LBA 0 is not a boot sector");
        return 0;
    }
    printf("BIOS: LBA 0 -> 7c00, ends in 55 aa; jmp 0000:7c00  DL=80\n");
    memcpy(low, at7c00, sizeof low);  /* start.S 가 하는 일: 자기를 0x0600 으로 옮긴다 */
    mbr_main(0x80, (const struct part *)(low + 0x1be));
    return 0;
}
