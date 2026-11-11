#!/bin/sh
# 같은 부트로더를 두 번 짓는다. 한 번은 이 기계의 컴파일러와 출력 백엔드로,
# 한 번은 RISC-V 교차 컴파일러와 진짜 백엔드로 짓고 QEMU 의 virt 기계에서 돌린다.
# 디스크 다섯을 단다. 파티션 표와 파일 시스템을 지은 것은 이 책이 아니라 남의 도구다:
#   1. QEMU 가 디렉터리 하나를 FAT 디스크로 보여 주는 기능(vvfat) --- MBR + FAT16
#   2. sfdisk 로 MBR 을, mkfs.fat 으로 FAT32 를, mcopy 로 파일을 --- MBR + FAT32
#   3. 같은 도구로 GPT 를 --- GPT + FAT32
#   4. 3번의 사본에서 앞 GPT 머리말을 망가뜨린 것 --- 짝 GPT 로 읽어야 한다
#   5. 같은 도구로 작은 디스크를 --- MBR + FAT12
# 파일 이름 boot-kernel.bin 은 8.3 에 들지 않아 도구가 긴 이름(LFN) 항목을 붙인다.
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
# 내용: 인사 한 줄 + 번호 붙은 줄 110000개(약 750 KiB). FAT12 디스크에서는 클러스터 약 380개에
# 걸쳐, 12비트 항목이 FAT 섹터 경계를 넘는 자리(341번 클러스터)를 지나 사슬을 따라가게 된다.
body() { printf 'hello from %s\n' "$1"; seq -f '%06g' 1 110000; }
body "QEMU vvfat (MBR, FAT16)" > "$dir/vv/boot-kernel.bin"
disks="-drive driver=vvfat,dir=$dir/vv,fat-type=16,if=none,id=d1,readonly=on -device virtio-blk-device,drive=d1"
tools="$ws/usr/toolchains/disk-tools/root"
if [ -x "$tools/usr/sbin/sfdisk" ]; then
    export LD_LIBRARY_PATH="$tools/usr/lib/x86_64-linux-gnu"
    # 이름 · 표 · 크기 · 파티션 종류 · FAT 비트 · 클러스터당 섹터 · 파일에 적을 글
    mk() {
        img="$dir/$1.img"
        truncate -s "$3" "$img"
        echo "start=2048, type=$4" | "$tools/usr/sbin/sfdisk" -q --label "$2" "$img"
        "$tools/usr/sbin/mkfs.fat" -F "$5" -s "$6" --offset 2048 "$img" >/dev/null
        body "sfdisk + mkfs.fat ($7)" > "$dir/k"
        MTOOLS_SKIP_CHECK=1 "$tools/usr/bin/mcopy" -i "$img@@1M" "$dir/k" ::boot-kernel.bin
    }
    attach() { disks="$disks -drive file=$dir/$1.img,format=raw,if=none,id=$1,readonly=on -device virtio-blk-device,drive=$1"; }
    mk mbr dos 64M c 32 1 "MBR, FAT32"                   # 종류 0x0c = FAT32(LBA)
    mk gpt gpt 64M U 32 1 "GPT, FAT32"                   # U = EFI 시스템 파티션
    mk f12 dos 4M  1 12 4 "MBR, FAT12"                   # 종류 0x01 = FAT12
    attach mbr; attach gpt
    # 4. 3번의 사본에서 앞 GPT 머리말의 한 바이트(디스크 GUID 안)를 망가뜨린다
    cp "$dir/gpt.img" "$dir/bad.img"
    printf '\377' | dd of="$dir/bad.img" bs=1 seek=$((512 + 56)) conv=notrunc 2>/dev/null
    attach bad; attach f12
fi
# shellcheck disable=SC2086
timeout 60 "$qemu" -machine virt -bios none -nographic -kernel ./rv.elf \
    -global virtio-mmio.force-legacy=false $disks
rm -rf ./rv.elf "$dir"
