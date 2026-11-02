#!/bin/bash
# LAB-03 buoc 1: build gdbserver va strace STATIC cho ARM
# (rootfs LAB-02 khong co libc nen ban dynamic se bao "not found")
set -e
T=$HOME/embedded_lab3/tools
GDBV=15.2
STV=6.11
J=$(nproc)
mkdir -p "$T" && cd "$T"
trap 'echo "!! LOI - xem log trong $T (*_configure.log, *_build.log) va gui cho minh 30 dong cuoi"' ERR

echo "== gdbserver $GDBV =="
[ -f gdb-$GDBV.tar.xz ] || wget -q --show-progress https://ftp.gnu.org/gnu/gdb/gdb-$GDBV.tar.xz
rm -rf gdb-$GDBV build-gdbserver && tar xf gdb-$GDBV.tar.xz
mkdir build-gdbserver && cd build-gdbserver
../gdb-$GDBV/configure --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf \
    --disable-gdb --disable-sim --disable-binutils --disable-ld --disable-gas \
    --disable-gprof --disable-gprofng --disable-gold --disable-readline \
    --disable-libdecnumber --disable-nls --disable-werror --enable-gdbserver \
    LDFLAGS="-static" > ../gdbserver_configure.log 2>&1
make -j"$J" all-gdbserver > ../gdbserver_build.log 2>&1
arm-linux-gnueabihf-strip -o "$T/gdbserver" gdbserver/gdbserver
cd "$T"

echo "== strace $STV =="
[ -f strace-$STV.tar.xz ] || wget -q --show-progress https://github.com/strace/strace/releases/download/v$STV/strace-$STV.tar.xz
rm -rf strace-$STV && tar xf strace-$STV.tar.xz && cd strace-$STV
./configure --host=arm-linux-gnueabihf --enable-mpers=no --enable-bundled=yes LDFLAGS="-static" \
    > ../strace_configure.log 2>&1
make -j"$J" > ../strace_build.log 2>&1
arm-linux-gnueabihf-strip -o "$T/strace" src/strace
cd "$T"

file "$T/gdbserver" "$T/strace"
echo "== Phai thay: ARM ... statically linked =="
