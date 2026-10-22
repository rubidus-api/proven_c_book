#!/bin/sh
# RISC-V bare metal --- run *on a real machine model*.
#
# ★ This example does not build with x86 gcc (even the `interrupt` attribute's argument differs).
#   That is the appendix's point too --- bare-metal C makes sense only after the target
#   machine is chosen. So it is built with a cross compiler and run on QEMU's virt machine.
# ★ Without the tools it is skipped, *and says so.* Silently, the reader would not know
#   this is code that has never run.
set -eu
cd "$(dirname "$0")"
ws=$(cd ../../.. && pwd)
rv="$ws/usr/toolchains/riscv-gcc/bin/riscv-none-elf-gcc"
qemu="$ws/usr/toolchains/qemu-riscv/bin/qemu-system-riscv64"

if [ ! -x "$rv" ] || [ ! -x "$qemu" ]; then
    echo "(no RISC-V toolchain here: this example was not built or run)"
    exit 0
fi

"$rv" -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -O2 -Wall -Wextra \
    -nostdlib -nostartfiles -T rv_virt.ld rv_start.S rv_timer.c -o ./rv.elf
timeout 60 "$qemu" -machine virt -bios none -nographic -kernel ./rv.elf
rm -f ./rv.elf
