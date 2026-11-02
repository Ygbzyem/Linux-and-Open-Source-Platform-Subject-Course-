# LAB-03 – Kernel Debugging & Embedded Application Performance

**English** | [Tiếng Việt](README.vi.md) · [← Home](../README.md)

GDB remote debugging, Valgrind, strace/perf and cyclictest on the QEMU vexpress-a9 target
built in LAB-01/LAB-02.

## Layout

| Path | Content |
|---|---|
| `apps/` | `buggy_app.c` (3 intentional bugs), `leaky_app.c` (leaks; `-DFIXED` builds the fixed version, `-DPAUSE_FOR_PROC` adds sleeps for `/proc` sampling), `perf_target.c` |
| `scripts/` | Numbered scripts 0–8 that set up, build, run and package the whole lab |
| `logs/` | Results: GDB session, Valgrind before/after, strace, perf, cyclictest histogram |

## Workflow

| Step | Script | What it does |
|---|---|---|
| 0 | `0_setup_host.sh` | Install gdb-multiarch, valgrind, strace, perf, rt-tests, gnuplot, stress-ng |
| 1 | `1_build_arm_tools.sh` | Build **static** ARM `gdbserver` and `strace` (the rootfs has no libc) |
| 2 | `2_build_apps.sh` | Build the apps for ARM (static) and x86_64 |
| 3 | `3_make_rootfs.sh` | LAB-02 rootfs + apps + tools + `net_up.sh`, `lab3_target.sh` → `initramfs_lab3.cpio.gz` |
| 4 | `4_boot_qemu.sh` | Boot QEMU with `hostfwd tcp::1234-:1234` (terminal 1) |
| 5 | `5_gdb_session.sh` | Scripted GDB session over all three bugs (terminal 2) |
| 6 | `6_host_tests.sh` | Part B (Valgrind) and Part C (strace, perf, flame graph) on the host |
| 7 | `7_partD_cyclictest.sh` | Part D: cyclictest under stress-ng load + gnuplot histogram |
| 8 | `8_package.sh` | Build the submission archive |

Inside QEMU: `net_up.sh; gdbserver :1234 /usr/bin/buggy_app` for Part A, `lab3_target.sh` for B.5/C.4.

## Deviations from the handout

| Handout | Here | Why |
|---|---|---|
| ARM apps built without `-static` | `-static` | The initramfs has no glibc, so dynamic binaries fail with `not found` |
| Host `gdbserver` package | Static ARM build from GDB source | The Ubuntu package is x86_64 |
| No network setup in the guest | `net_up.sh` (10.0.2.15) | `hostfwd` needs the guest NIC configured |
| `'~/…'` paths in gnuplot | Absolute paths | gnuplot does not expand `~` |
| `leaky_app` exits in milliseconds | Optional `PAUSE_FOR_PROC` sleeps | Lets `/proc/PID/status` be read before and after the leak |

## Results

Measured numbers for every part: [`RESULTS.md`](RESULTS.md).
