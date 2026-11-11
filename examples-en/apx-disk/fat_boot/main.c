/* One job of a bootloader: on each disk, read the partition table, and from the root
   directory of a FAT partition load boot-kernel.bin into memory. The eight virtio-mmio
   windows (from 0x1000_1000, 0x1000 apart) are the values in the device tree QEMU
   provides. */
#include "part.h"
#include "fat.h"
#include "hw.h"

#define FINISH 0x00100000u
#define BOOT_FILE "boot-kernel.bin"   /* not an 8.3 name: found only by its long name */

static uint8_t kernel[1024 * 1024];          /* where the kernel is loaded */

/* Check what was loaded. After a first line (a greeting), the file is lines numbered in
   order from "000001\n". One wrong step along the cluster chain breaks the numbering.
   Returns the line count if correct. */
static long numbered_lines(const uint8_t *p, long len)
{
    long i = 0, n = 0;
    while (i < len && p[i] != '\n') i++;              /* skip the first line */
    for (i++; i + 7 <= len; i += 7) {
        long v = 0;
        for (int k = 0; k < 6; k++) v = v * 10 + (p[i + k] - '0');
        if (v != ++n || p[i + 6] != '\n') return -1;
    }
    return i == len ? n : -1;
}

/* A GPT GUID as text: the first three groups are stored little-endian, so they are read
   reversed. */
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

        struct fat fs;  /* do not trust the type marker; look */
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
        kernel[first] = 0;                          /* print only the first line */
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
