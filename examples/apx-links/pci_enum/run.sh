#!/bin/sh
# 같은 드라이버를 두 번 짓는다. 한 번은 이 기계의 컴파일러와 출력 백엔드로,
# 한 번은 RISC-V 교차 컴파일러와 진짜 백엔드로 짓고 QEMU 의 virt 기계에서 돌린다.
set -eu
cd "$(dirname "$0")"
hw=../../apx-riscv
cc=${CC:-gcc}

echo "== this machine: the commands the driver sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -I. -I$hw -o ./trace main.c pci.c $hw/hw_trace.c
./trace
rm -f ./trace

echo
echo "== QEMU virt (emulated RISC-V): the same driver on the PCIe host bridge model =="
ws=$(cd ../../../.. && pwd)
rv="$ws/usr/toolchains/riscv-gcc/bin/riscv-none-elf-gcc"
qemu="$ws/usr/toolchains/qemu-riscv/bin/qemu-system-riscv64"
if [ ! -x "$rv" ] || [ ! -x "$qemu" ]; then
    echo "(no RISC-V toolchain here: this half was not built or run)"
    exit 0
fi
"$rv" -std=c23 -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -O2 -Wall -Wextra -Werror \
    -ffreestanding -nostdlib -nostartfiles -Wl,--no-warn-rwx-segments -I. -I$hw -T $hw/rv_virt.ld \
    $hw/rv_start.S main.c pci.c $hw/hw_mmio.c -o ./rv.elf
timeout 60 "$qemu" -machine virt -bios none -nographic -kernel ./rv.elf -device edu -device virtio-rng-pci
rm -f ./rv.elf
