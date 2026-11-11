#!/bin/sh
# This driver runs with the print backend only. QEMU's RISC-V machines have no I2C controller model.
set -eu
cd "$(dirname "$0")"
hw=../../apx-riscv
cc=${CC:-gcc}

echo "== this machine: the commands the driver sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -I. -I$hw -o ./trace main.c i2c.c $hw/hw_trace.c
./trace
rm -f ./trace
echo
echo "(QEMU's RISC-V machines model no I2C controller: there is no second half here)"
