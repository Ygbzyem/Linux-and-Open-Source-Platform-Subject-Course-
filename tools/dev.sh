#!/bin/bash
# dev.sh - build -> copy vao rootfs -> dong goi initramfs -> boot QEMU
# Cach dung: cd vao thu muc co Makefile cua driver, roi chay dev.sh
set -e
shopt -s nullglob

R=$HOME/embedded_lab2/rootfs/initramfs
OUT=$HOME/embedded_lab2/output/initramfs_lab2.cpio.gz
KVER=5.15.221

if [ ! -f Makefile ]; then
    echo "LOI: khong thay Makefile trong $(pwd). Hay cd vao thu muc driver truoc." >&2
    exit 1
fi

sudo -v

echo "== [2] Build =="
make

echo "== [3] Copy vao rootfs =="
sudo mkdir -p "$R/lib/modules/$KVER"
for f in *.ko; do
    sudo cp -- "$f" "$R/lib/modules/$KVER/"
    echo "  module : $f"
done
for f in *.sh; do
    sudo install -m 755 -- "$f" "$R/usr/bin/"
    echo "  script : $f"
done
for f in *; do
    if [ -f "$f" ] && [ -x "$f" ] && file -- "$f" | grep -q 'ARM.*statically linked'; then
        sudo install -m 755 -- "$f" "$R/usr/bin/"
        echo "  app    : $f"
    fi
done

echo "== [4] Dong goi initramfs =="
( cd "$R" && find . | sudo cpio -H newc -o 2>/dev/null | gzip > "$OUT" )
echo "  Kiem tra trong goi:"
zcat "$OUT" | cpio -t 2>/dev/null | grep -E '\.ko$|usr/bin/(test_|asgn|counter|sms)' | sed 's/^/    /'

echo "== [5] Boot QEMU (thoat: Ctrl+A roi X) =="
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel "$HOME/embedded_lab1/output/zImage" \
  -dtb "$HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb" \
  -initrd "$OUT" \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init'
