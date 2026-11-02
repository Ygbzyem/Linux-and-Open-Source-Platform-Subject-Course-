#!/bin/bash
# LAB-03 buoc 0: cai cong cu tren host + tao thu muc + chep source
set -e
KIT=$(cd "$(dirname "$0")" && pwd)
L3=$HOME/embedded_lab3
sudo apt update
sudo apt install -y gdb-multiarch valgrind strace rt-tests gnuplot stress-ng \
     g++-arm-linux-gnueabihf build-essential wget xz-utils git time
sudo apt install -y "linux-tools-$(uname -r)" || sudo apt install -y linux-tools-generic
mkdir -p "$L3"/{apps,rootfs,output,logs,tools,rt_kernel}
cp "$KIT"/apps/*.c "$L3/apps/"
echo "== Phien ban cong cu ==" | tee "$L3/logs/tool_versions.txt"
{ uname -a; gdb-multiarch --version | head -1; valgrind --version; strace -V | head -1;
  perf --version 2>&1 | head -1; cyclictest --help 2>&1 | head -1;
  arm-linux-gnueabihf-gcc --version | head -1; qemu-system-arm --version | head -1; } \
  | tee -a "$L3/logs/tool_versions.txt"
