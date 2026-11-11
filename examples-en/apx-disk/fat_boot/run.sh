#!/bin/sh
# Build the same bootloader twice: once with this machine's compiler and the print backend,
# once with the RISC-V cross compiler and the real backend, run on QEMU's virt machine.
# Three disks are attached. The partition tables and file systems are made by other people's tools, not this book:
#   1. QEMU's feature that shows a directory as a FAT disk (vvfat) --- MBR + FAT16
#   2. MBR by sfdisk, FAT32 by mkfs.fat, the file by mcopy --- MBR + FAT32
#   3. GPT by the same tools --- GPT + FAT32
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
echo "== QEMU virt (emulated RISC-V): the same loader on three virtio disks =="
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
printf 'hello from QEMU vvfat (MBR, FAT16)\n' > "$dir/vv/KERNEL.BIN"
disks="-drive driver=vvfat,dir=$dir/vv,fat-type=16,if=none,id=d1,readonly=on -device virtio-blk-device,drive=d1"
tools="$ws/usr/toolchains/disk-tools/root"
if [ -x "$tools/usr/sbin/sfdisk" ]; then
    export LD_LIBRARY_PATH="$tools/usr/lib/x86_64-linux-gnu"
    for label in dos gpt; do
        img="$dir/$label.img"
        truncate -s 64M "$img"
        case $label in
            dos) echo 'start=2048, type=c' ;;            # FAT32(LBA)
            gpt) echo 'start=2048, type=U' ;;            # EFI system partition
        esac | "$tools/usr/sbin/sfdisk" -q --label $label "$img"
        "$tools/usr/sbin/mkfs.fat" -F 32 -s 1 --offset 2048 "$img" >/dev/null
        printf 'hello from sfdisk + mkfs.fat (%s, FAT32)\n' "$(echo $label | sed 's/dos/MBR/;s/gpt/GPT/')" > "$dir/k"
        MTOOLS_SKIP_CHECK=1 "$tools/usr/bin/mcopy" -i "$img@@1M" "$dir/k" ::KERNEL.BIN
        disks="$disks -drive file=$img,format=raw,if=none,id=$label,readonly=on -device virtio-blk-device,drive=$label"
    done
fi
# shellcheck disable=SC2086
timeout 60 "$qemu" -machine virt -bios none -nographic -kernel ./rv.elf \
    -global virtio-mmio.force-legacy=false $disks
rm -rf ./rv.elf "$dir"
