#!/bin/bash
set -e
cd ~/embedded_lab2/rootfs/initramfs
find . | sudo cpio -H newc -o | gzip > ~/embedded_lab2/output/initramfs_lab2.cpio.gz
echo "--- Noi dung lien quan trong goi ---"
zcat ~/embedded_lab2/output/initramfs_lab2.cpio.gz | cpio -t 2>/dev/null | grep -E 'lab2|test_|rcS|inittab'
