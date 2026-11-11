/* 부트로더의 일 하나: 디스크마다 파티션 표를 읽고, FAT 파티션의 뿌리 디렉터리에서
   boot-kernel.bin 을 기억 장치로 읽어 들인다. virtio-mmio 창 여덟 칸(0x1000_1000 부터
   0x1000 간격)은 QEMU 가 내주는 장치 트리에 적힌 값이다. */
#include "part.h"
#include "fat.h"
#include "hw.h"

#define FINISH 0x00100000u
#define BOOT_FILE "boot-kernel.bin"   /* 8.3 에 들지 않는 이름: 긴 이름으로만 찾힌다 */

static uint8_t kernel[1024 * 1024];          /* 커널을 올려 둘 자리 */

/* 올린 내용이 맞는지 본다. 파일은 첫 줄(인사) 뒤에 "000001\n" 부터 차례로 번호를 단
   줄들이다. 클러스터 사슬을 하나라도 잘못 따라가면 번호가 끊긴다. 맞으면 줄 수를 돌려준다. */
static long numbered_lines(const uint8_t *p, long len)
{
    long i = 0, n = 0;
    while (i < len && p[i] != '\n') i++;              /* 첫 줄은 건너뛴다 */
    for (i++; i + 7 <= len; i += 7) {
        long v = 0;
        for (int k = 0; k < 6; k++) v = v * 10 + (p[i + k] - '0');
        if (v != ++n || p[i + 6] != '\n') return -1;
    }
    return i == len ? n : -1;
}

/* GPT 의 GUID 를 글로: 앞의 세 마디는 작은 끝으로 놓여 있어 뒤집어 읽는다. */
static void log_guid(const uint8_t g[16])
{
    static const int order[16] = { 3, 2, 1, 0, -1, 5, 4, -1, 7, 6, -1, 8, 9, -1, 10, 11 };
    for (int i = 0; i < 16; i++) {
        if (order[i] < 0) { hw_log("-"); continue; }
        hw_log_hex(g[order[i]], 2);
    }
    for (int i = 12; i < 16; i++) hw_log_hex(g[i], 2);
}

static void boot_from(struct vblk *disk)
{
    struct part p[8];
    uint64_t gpt_at;
    int n = partitions(disk, p, 8, &gpt_at);
    if (n == -2) { hw_log("  sector 0: the device did not answer\n"); return; }
    if (n < 0) { hw_log("  no partition table that passes the checks\n"); return; }
    if (gpt_at > 1) {
        hw_log("  primary GPT failed its checks; using the backup at sector ");
        hw_log_dec(gpt_at); hw_log("\n");
    }
    for (int i = 0; i < n; i++) {
        hw_log(p[i].scheme == 'G' ? "  GPT partition " : "  MBR partition ");
        hw_log_dec((uint64_t)i + 1); hw_log(": sectors "); hw_log_dec(p[i].lba);
        hw_log(" + "); hw_log_dec(p[i].count);
        if (p[i].scheme == 'G') { hw_log(", type "); log_guid(p[i].guid); }
        else { hw_log(", type 0x"); hw_log_hex(p[i].mbr_type, 2); }
        hw_log("\n");

        struct fat fs;                              /* 종류 표시는 믿지 않고 직접 본다 */
        if (fat_mount(&fs, disk, (uint32_t)p[i].lba) != 0) continue;
        hw_log("    FAT"); hw_log_dec(fs.bits); hw_log(": "); hw_log_dec(fs.clusters);
        hw_log(" clusters of "); hw_log_dec(fs.spc); hw_log(" sectors, FAT at sector ");
        hw_log_dec(fs.fat_lba); hw_log(", data at sector "); hw_log_dec(fs.data_lba); hw_log("\n");
        long len = fat_load(&fs, BOOT_FILE, kernel, sizeof kernel - 1);
        if (len == -2) { hw_log("    " BOOT_FILE ": cluster chain broken\n"); continue; }
        if (len < 0) { hw_log("    " BOOT_FILE " not found\n"); continue; }
        long lines = numbered_lines(kernel, len);
        long first = 0;
        while (first < len && kernel[first] != '\n') first++;
        kernel[first] = 0;                          /* 첫 줄만 찍는다 */
        hw_log("    " BOOT_FILE ", "); hw_log_dec((uint64_t)len); hw_log(" bytes: ");
        hw_log((const char *)kernel);
        if (lines < 0) hw_log("\n    numbered lines broken\n");
        else { hw_log("\n    "); hw_log_dec((uint64_t)lines); hw_log(" numbered lines, all in order\n"); }
    }
}

int main(void)
{
    for (uintptr_t a = 0x10001000u; a <= 0x10008000u; a += 0x1000) {
        struct vblk disk = { .base = a };
        if (vblk_init(&disk) != 0) continue;
        hw_log("virtio-blk at 0x"); hw_log_hex(a, 8);
        hw_log(", "); hw_log_dec(disk.sectors); hw_log(" sectors\n");
        boot_from(&disk);
    }
    hw_write32(FINISH, 0x5555u);
    return 0;
}
