/* One job of a bootloader: on each disk, read the partition table, and from the root
   directory of a FAT partition load KERNEL.BIN into memory. The eight virtio-mmio
   windows (from 0x1000_1000, 0x1000 apart) are the values in the device tree QEMU
   provides. */
#include "part.h"
#include "fat.h"
#include "hw.h"

#define FINISH 0x00100000u

static uint8_t kernel[64 * 1024];            /* where the kernel is loaded */

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
    int n = partitions(disk, p, 8);
    if (n == -2) { hw_log("  sector 0: the device did not answer\n"); return; }
    if (n < 0) { hw_log("  no partition table that passes the checks\n"); return; }
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
        long len = fat_load(&fs, "KERNEL  BIN", kernel, sizeof kernel - 1);
        if (len < 0) { hw_log("    KERNEL.BIN not found\n"); continue; }
        kernel[len] = 0;
        hw_log("    KERNEL.BIN, "); hw_log_dec((uint64_t)len); hw_log(" bytes: ");
        hw_log((const char *)kernel);
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
