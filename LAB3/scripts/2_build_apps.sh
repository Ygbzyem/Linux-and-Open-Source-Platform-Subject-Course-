#!/bin/bash
# LAB-03 buoc 2: build 3 ung dung cho ARM (static) va host x86_64
set -e
cd "$HOME/embedded_lab3/apps"
CC=arm-linux-gnueabihf-gcc
ARM="-march=armv7-a -mfpu=neon-vfpv3 -mfloat-abi=hard -static"

$CC -g -O0 $ARM -o buggy_app buggy_app.c                  # Phan A (canh bao use-after-free la BUG CO Y)
gcc  -g -O0 -o leaky_app leaky_app.c                       # Phan B: ban co leak
gcc  -g -O0 -DFIXED -o leaky_app_fixed leaky_app.c         # Phan B: ban da sua (bat 3 lenh free)
$CC -g -O0 $ARM -DPAUSE_FOR_PROC -o leaky_app_arm leaky_app.c   # Phan B.5: co sleep de doc /proc
gcc  -g -O1 -o perf_target perf_target.c -lm               # Phan C host
$CC -g -O1 $ARM -o perf_target_arm perf_target.c -lm       # Phan C.4 ARM

file buggy_app leaky_app leaky_app_fixed leaky_app_arm perf_target perf_target_arm
arm-linux-gnueabihf-size buggy_app | tee "$HOME/embedded_lab3/logs/buggy_app_size.txt"
