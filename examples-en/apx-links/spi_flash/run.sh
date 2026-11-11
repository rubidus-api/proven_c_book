#!/bin/sh
# Build the same driver twice: once with this machine's compiler and the print backend,
# once with the RISC-V cross compiler and the real backend, run on QEMU's sifive_u machine.
# The flash contents are a 32 MiB file handed to QEMU, with eight bytes written at the front.
set -eu
cd "$(dirname "$0")"
hw=../../apx-riscv
cc=${CC:-gcc}

echo "== this machine: the commands the driver sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -I. -I$hw -o ./trace main.c spi.c $hw/hw_trace.c
./trace
rm -f ./trace

echo
echo "== QEMU sifive_u (emulated RISC-V): the same driver on the SPI controller and flash models =="
ws=$(cd ../../../.. && pwd)
rv="$ws/usr/toolchains/riscv-gcc/bin/riscv-none-elf-gcc"
qemu="$ws/usr/toolchains/qemu-riscv/bin/qemu-system-riscv64"
if [ ! -x "$rv" ] || [ ! -x "$qemu" ]; then
    echo "(no RISC-V toolchain here: this half was not built or run)"
    exit 0
fi
"$rv" -std=c23 -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -O2 -Wall -Wextra -Werror \
    -ffreestanding -nostdlib -nostartfiles -Wl,--no-warn-rwx-segments -DBOARD_SIFIVE_U \
    -I. -I$hw -T $hw/rv_virt.ld $hw/rv_start.S main.c spi.c $hw/hw_mmio.c -o ./rv.elf
img=$(mktemp)
printf 'BOOT OK\n' > "$img"
truncate -s 32M "$img"
timeout 60 "$qemu" -machine sifive_u -bios none -nographic -kernel ./rv.elf \
    -semihosting-config enable=on,target=native \
    -drive file="$img",format=raw,if=mtd
rm -f ./rv.elf "$img"
