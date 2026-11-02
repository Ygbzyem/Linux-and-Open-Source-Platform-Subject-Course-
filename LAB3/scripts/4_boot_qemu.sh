#!/bin/bash
# LAB-03 buoc 4: boot QEMU, forward cong 1234 host -> 1234 guest. Ghi log terminal.
L3=$HOME/embedded_lab3
script -q -c "qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel $HOME/embedded_lab1/output/zImage \
  -dtb $HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd $L3/output/initramfs_lab3.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M' \
  -nic user,model=lan9118,hostfwd=tcp::1234-:1234" "$L3/logs/qemu_session.txt"
