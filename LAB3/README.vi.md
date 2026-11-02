# LAB-03 – Debug kernel & Phân tích hiệu năng ứng dụng nhúng

[English](README.md) | **Tiếng Việt** · [← Trang chủ](../README.vi.md)

GDB remote debug, Valgrind, strace/perf và cyclictest trên target QEMU vexpress-a9 dựng ở
LAB-01/LAB-02.

## Cấu trúc

| Đường dẫn | Nội dung |
|---|---|
| `apps/` | `buggy_app.c` (3 bug cố ý), `leaky_app.c` (có leak; `-DFIXED` build bản đã sửa, `-DPAUSE_FOR_PROC` thêm sleep để đo `/proc`), `perf_target.c` |
| `scripts/` | Script đánh số 0–8: cài đặt, build, chạy và đóng gói toàn bộ bài lab |
| `logs/` | Kết quả: phiên GDB, Valgrind trước/sau sửa, strace, perf, histogram cyclictest |

## Quy trình

| Bước | Script | Việc làm |
|---|---|---|
| 0 | `0_setup_host.sh` | Cài gdb-multiarch, valgrind, strace, perf, rt-tests, gnuplot, stress-ng |
| 1 | `1_build_arm_tools.sh` | Build `gdbserver` và `strace` **static** cho ARM (rootfs không có libc) |
| 2 | `2_build_apps.sh` | Build app cho ARM (static) và x86_64 |
| 3 | `3_make_rootfs.sh` | Rootfs LAB-02 + app + tool + `net_up.sh`, `lab3_target.sh` → `initramfs_lab3.cpio.gz` |
| 4 | `4_boot_qemu.sh` | Boot QEMU với `hostfwd tcp::1234-:1234` (terminal 1) |
| 5 | `5_gdb_session.sh` | Phiên GDB tự động qua cả 3 bug (terminal 2) |
| 6 | `6_host_tests.sh` | Phần B (Valgrind) và Phần C (strace, perf, flame graph) trên host |
| 7 | `7_partD_cyclictest.sh` | Phần D: cyclictest có tải stress-ng + histogram gnuplot |
| 8 | `8_package.sh` | Đóng gói bài nộp |

Trong QEMU: `net_up.sh; gdbserver :1234 /usr/bin/buggy_app` cho Phần A, `lab3_target.sh` cho B.5/C.4.

## Khác với đề và lý do

| Đề | Ở đây | Lý do |
|---|---|---|
| App ARM build không có `-static` | Có `-static` | Initramfs không có glibc, binary dynamic báo `not found` |
| Dùng gói `gdbserver` của host | Build gdbserver static cho ARM từ source GDB | Gói Ubuntu là bản x86_64 |
| Không cấu hình mạng trong guest | `net_up.sh` (10.0.2.15) | `hostfwd` cần card mạng guest có IP |
| Đường dẫn `'~/…'` trong gnuplot | Đường dẫn đầy đủ | gnuplot không hiểu `~` |
| `leaky_app` chạy xong trong vài ms | Tuỳ chọn `PAUSE_FOR_PROC` thêm sleep | Kịp đọc `/proc/PID/status` trước và sau khi leak |
