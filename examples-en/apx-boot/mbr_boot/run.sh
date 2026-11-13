#!/bin/sh
# The same boot code (MBR and VBR) is built twice: once with this machine's compiler and the
# print backend, once as 16-bit real-mode code (gcc -m16) with the real backend, booted by the
# BIOS on QEMU's PC machine.
# Three disk images are made in a temporary directory. The partition tables are written not by
# this book but by sfdisk:
#   1. an MBR table (sfdisk --label dos): a Linux partition marked for booting, plus swap
#   2. the same table, but no VBR written at the start of the partition --- no 55 aa there
#   3. GPT (sfdisk --label gpt): sector 0 becomes the protective MBR
# This MBR goes into the first 440 bytes of all three, the VBR at the start of image 1's first
# partition. Nothing is written to a device --- only to files in the temporary directory.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
ws=$(cd ../../../.. && pwd)
tools="$ws/usr/toolchains/disk-tools/root"
if command -v sfdisk >/dev/null 2>&1; then sfdisk=sfdisk
elif [ -x "$tools/usr/sbin/sfdisk" ]; then
    sfdisk="$tools/usr/sbin/sfdisk"; export LD_LIBRARY_PATH="$tools/usr/lib/x86_64-linux-gnu"
else
    echo "(no sfdisk here: no disk image, so nothing was built or run)"; exit 0
fi
if ! echo 'int x;' | $cc -m16 -c -x c -o /dev/null - 2>/dev/null; then
    echo "(this compiler cannot make 16-bit x86 code: nothing was built or run)"; exit 0
fi

dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
# The 16-bit real-mode build. The MBR is linked at 0x0600, the VBR at 0x7c00.
cf="-std=c23 -m16 -march=i386 -Os -ffreestanding -fno-pic -fno-asynchronous-unwind-tables
    -fno-stack-protector -mregparm=3 -Wall -Wextra -Werror"
# shellcheck disable=SC2086
(cd "$dir" && for s in start.S vstart.S mbr.c vboot.c vbr.c bios_int.c; do
    $cc $cf -c "$OLDPWD/$s" -o "${s%.*}.o"; done)
ld -m elf_i386 --no-warn-rwx-segments -T mbr.ld -o "$dir/mbr.elf" "$dir/start.o" "$dir/mbr.o" "$dir/bios_int.o"
(cd "$dir" && ld -m elf_i386 --no-warn-rwx-segments -T "$OLDPWD/vbr.ld" -o vbr.elf vstart.o vboot.o bios_int.o vbr.o)
objcopy -O binary "$dir/mbr.elf" "$dir/mbr.bin"
objcopy -O binary -R .bss "$dir/vbr.elf" "$dir/vbr.bin"
n=$(wc -c < "$dir/mbr.bin")
if [ "$n" -gt 440 ]; then echo "MBR code is $n bytes; only 440 fit before the disk signature"; exit 1; fi
echo "MBR code: $n of the 440 bytes before the disk signature"

mk() {   # name, table format, the table to give sfdisk
    img="$dir/$1.img"
    truncate -s 8M "$img"
    dd if="$dir/mbr.bin" of="$img" conv=notrunc 2>/dev/null
    printf '%b' "$3" | "$sfdisk" -q --label "$2" "$img"
    # did sfdisk leave the code alone? (the disk signature starts at 0x1b8)
    cmp -s -n "$n" "$dir/mbr.bin" "$img" || { echo "sfdisk overwrote the boot code"; exit 1; }
}
mk mbr dos 'label-id: 0x50524f56\nstart=2048, size=4096, type=83, bootable\nstart=6144, type=82\n'
dd if="$dir/vbr.bin" of="$dir/mbr.img" bs=512 seek=2048 conv=notrunc 2>/dev/null
mk novbr dos 'label-id: 0x50524f56\nstart=2048, size=4096, type=83, bootable\nstart=6144, type=82\n'
mk gpt gpt 'label-id: 50524F56-0000-4000-8000-000000000001\nstart=2048, size=4096, type=L\n'

echo "== this machine: the commands the boot code sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -I. -o "$dir/trace" mbr.c vbr.c bios_trace.c
for d in mbr novbr gpt; do echo "-- $d.img"; "$dir/trace" "$dir/$d.img"; done

echo
echo "== QEMU PC (emulated x86, SeaBIOS): the same code booted by the BIOS =="
q="$ws/usr/toolchains/qemu-x86/root"
qemu="$q/usr/bin/qemu-system-i386"
if [ ! -x "$qemu" ]; then echo "(no QEMU for x86 here: this half was not run)"; exit 0; fi
for d in mbr novbr gpt; do
    echo "-- $d.img"
    rc=0
    LD_LIBRARY_PATH="$q/usr/lib/x86_64-linux-gnu" timeout 30 "$qemu" -L "$q/usr/share/seabios" \
        -L "$q/usr/share/qemu" -display none -serial stdio -monitor none -nic none \
        -drive file="$dir/$d.img",format=raw -device isa-debug-exit,iobase=0xf4 > "$dir/out" || rc=$?
    tr -d '\r' < "$dir/out"
    # isa-debug-exit ends with (value << 1) | 1: 1 for stop(0), 3 for stop(1)
    case $rc in 1) echo "(stopped: reached the end)" ;; 3) echo "(stopped: the boot code gave up)" ;;
        *) echo "QEMU ended with $rc"; exit 1 ;; esac
done
