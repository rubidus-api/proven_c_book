#!/bin/sh
# Build the same bootloader twice: once with this machine's compiler and the print backend,
# once with the RISC-V cross compiler and the real backend, run on QEMU's virt machine.
# Five disks are attached. The partition tables and file systems are made by other people's tools, not this book:
#   1. QEMU's feature that shows a directory as a FAT disk (vvfat) --- MBR + FAT16
#   2. MBR by sfdisk, FAT32 by mkfs.fat, the file by mcopy --- MBR + FAT32
#   3. GPT by the same tools --- GPT + FAT32
#   4. a copy of disk 3 with the primary GPT header damaged --- must be read from the backup
#   5. a small disk by the same tools --- MBR + FAT12
# The file name boot-kernel.bin does not fit 8.3, so the tools add long-name (LFN) entries.
# Without those tools only disk 1 is attached.
set -eu
cd "$(dirname "$0")"
hw=../../apx-riscv
cc=${CC:-gcc}

echo "== this machine: the commands the driver sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -no-pie -I. -I$hw -o ./trace main.c part.c fat.c virtio_blk.c $hw/hw_trace.c
./trace
rm -f ./trace

echo
echo "== QEMU virt (emulated RISC-V): the same loader on five virtio disks =="
ws=$(cd ../../../.. && pwd)
rv="$ws/usr/toolchains/riscv-gcc/bin/riscv-none-elf-gcc"
qemu="$ws/usr/toolchains/qemu-riscv/bin/qemu-system-riscv64"
if [ ! -x "$rv" ] || [ ! -x "$qemu" ]; then
    echo "(no RISC-V toolchain here: this half was not built or run)"
    exit 0
fi
"$rv" -std=c23 -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -O2 -Wall -Wextra -Werror \
    -ffreestanding -nostdlib -nostartfiles -Wl,--no-warn-rwx-segments -I. -I$hw -T $hw/rv_virt.ld \
    $hw/rv_start.S main.c part.c fat.c virtio_blk.c $hw/hw_mmio.c -o ./rv.elf
dir=$(mktemp -d)
mkdir "$dir/vv"
# Contents: a greeting + 110000 numbered lines (about 750 KiB). On the FAT12 disk this spans about
# 380 clusters, so the chain is followed past the 12-bit entry that straddles a FAT sector (cluster 341).
body() { printf 'hello from %s\n' "$1"; seq -f '%06g' 1 110000; }
body "QEMU vvfat (MBR, FAT16)" > "$dir/vv/boot-kernel.bin"
disks="-drive driver=vvfat,dir=$dir/vv,fat-type=16,if=none,id=d1,readonly=on -device virtio-blk-device,drive=d1"
tools="$ws/usr/toolchains/disk-tools/root"
if [ -x "$tools/usr/sbin/sfdisk" ]; then
    export LD_LIBRARY_PATH="$tools/usr/lib/x86_64-linux-gnu"
    # name, label, size, partition type, FAT bits, sectors per cluster, text for the file
    mk() {
        img="$dir/$1.img"
        truncate -s "$3" "$img"
        echo "start=2048, type=$4" | "$tools/usr/sbin/sfdisk" -q --label "$2" "$img"
        "$tools/usr/sbin/mkfs.fat" -F "$5" -s "$6" --offset 2048 "$img" >/dev/null
        body "sfdisk + mkfs.fat ($7)" > "$dir/k"
        MTOOLS_SKIP_CHECK=1 "$tools/usr/bin/mcopy" -i "$img@@1M" "$dir/k" ::boot-kernel.bin
    }
    attach() { disks="$disks -drive file=$dir/$1.img,format=raw,if=none,id=$1,readonly=on -device virtio-blk-device,drive=$1"; }
    mk mbr dos 64M c 32 1 "MBR, FAT32"                   # type 0x0c = FAT32 (LBA)
    mk gpt gpt 64M U 32 1 "GPT, FAT32"                   # U = EFI system partition
    mk f12 dos 4M  1 12 4 "MBR, FAT12"                   # type 0x01 = FAT12
    attach mbr; attach gpt
    # 4. a copy of disk 3 with one byte of the primary GPT header (inside the disk GUID) damaged
    cp "$dir/gpt.img" "$dir/bad.img"
    printf '\377' | dd of="$dir/bad.img" bs=1 seek=$((512 + 56)) conv=notrunc 2>/dev/null
    attach bad; attach f12
fi
# shellcheck disable=SC2086
timeout 60 "$qemu" -machine virt -bios none -nographic -kernel ./rv.elf \
    -global virtio-mmio.force-legacy=false $disks
rm -rf ./rv.elf "$dir"
