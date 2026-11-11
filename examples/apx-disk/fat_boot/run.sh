#!/bin/sh
# 같은 부트로더를 두 번 짓는다. 한 번은 이 기계의 컴파일러와 출력 백엔드로,
# 한 번은 RISC-V 교차 컴파일러와 진짜 백엔드로 짓고 QEMU 의 virt 기계에서 돌린다.
# 디스크 셋을 단다. 파티션 표와 파일 시스템을 지은 것은 이 책이 아니라 남의 도구다:
#   1. QEMU 가 디렉터리 하나를 FAT 디스크로 보여 주는 기능(vvfat) --- MBR + FAT16
#   2. sfdisk 로 MBR 을, mkfs.fat 으로 FAT32 를, mcopy 로 파일을 --- MBR + FAT32
#   3. 같은 도구로 GPT 를 --- GPT + FAT32
# 도구가 없으면 1번만 단다.
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
            gpt) echo 'start=2048, type=U' ;;            # EFI 시스템 파티션
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
