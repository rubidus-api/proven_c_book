#!/bin/sh
# Build the same driver twice: once with this machine's compiler and the print backend,
# once with the RISC-V cross compiler and the real backend, run on QEMU's virt machine.
set -eu
cd "$(dirname "$0")"
hw=../../apx-riscv
cc=${CC:-gcc}

echo "== this machine: the commands the driver sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -I. -I$hw -o ./trace main.c uart16550.c $hw/hw_trace.c
./trace
rm -f ./trace

echo
echo "== QEMU virt (emulated RISC-V): the same driver on the 16550 model =="
ws=$(cd ../../../.. && pwd)
rv="$ws/usr/toolchains/riscv-gcc/bin/riscv-none-elf-gcc"
qemu="$ws/usr/toolchains/qemu-riscv/bin/qemu-system-riscv64"
if [ ! -x "$rv" ] || [ ! -x "$qemu" ]; then
    echo "(no RISC-V toolchain here: this half was not built or run)"
    exit 0
fi
"$rv" -std=c23 -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -O2 -Wall -Wextra -Werror \
    -ffreestanding -nostdlib -nostartfiles -Wl,--no-warn-rwx-segments -I. -I$hw -T $hw/rv_virt.ld \
    $hw/rv_start.S main.c uart16550.c $hw/hw_mmio.c -o ./rv.elf
timeout 60 "$qemu" -machine virt -bios none -nographic -kernel ./rv.elf
rm -f ./rv.elf
