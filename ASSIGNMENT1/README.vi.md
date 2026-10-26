# ASSIGNMENT-01 – How to Write a Device Driver

[English](README.md) | **Tiếng Việt** · [← Về trang chủ](../README.vi.md)

| Task | Nội dung | Điểm | Trạng thái |
|---|---|---|---|
| 1 | Mở rộng driver LAB-02 với **ioctl** (4 lệnh) | 40 | ✅ 24/24 test PASS |
| 2 | **Misc device** `counter_driver` viết mới | 30 | ✅ 25/25 test PASS |
| 3 | Báo cáo phân tích thiết kế | 20 | – |
| 4 | 4 câu hỏi lý thuyết | 10 | ✅ Xem [docs/07](../docs/vi/07-van-dap.md) mục C1, E1, F, G1 |

---

## 📁 Nội dung thư mục

```text
ASSIGNMENT1/
├── task1_ioctl/
│   ├── asgn1_ioctl.h      ← "hợp đồng" chung kernel ↔ userspace: magic, struct, 4 lệnh
│   ├── asgn1_driver.c     ← driver: char device + procfs + sysfs + unlocked_ioctl
│   ├── asgn1_app.c        ← chương trình userspace gọi ioctl
│   ├── test_asgn1.sh      ← test tự động, in PASS/FAIL
│   └── Makefile           ← build cả module (.ko) và app (ARM, static)
├── task2_misc/
│   ├── counter_driver.c   ← misc device /dev/counter + /proc/counter_info
│   ├── test_counter.sh    ← test tự động, in PASS/FAIL
│   └── Makefile
└── logs/
    ├── test_asgn1_output.txt
    └── test_counter_output.txt
```

---

# Task 1 – ioctl interface

## 🏗️ Kiến trúc

```text
 USERSPACE                                     KERNEL
 ┌────────────────────┐                         ┌──────────────────────────────────────┐
 │ echo / cat         │── read()/write() ──────►│ asgn1_read / asgn1_write             │
 │                    │                         │   mode KERNEL → thêm "[KERNEL] "     │
 │ asgn1_app          │── ioctl(fd, cmd, &x) ──►│ asgn1_ioctl                          │
 │   reset | stats    │                         │   switch(cmd): RESET / GET_STATS /   │
 │   mode N | version │◄── copy_to_user ────────│   SET_MODE / GET_VERSION / -ENOTTY   │
 └─────────┬──────────┘                         │                                      │
           │ #include                           │ dữ liệu chung: device_buffer, counters│
           ▼                                    │ mode – tất cả bảo vệ bởi asgn1_mutex │
   ┌───────────────┐   #include                 │                                      │
   │ asgn1_ioctl.h │◄───────────────────────────│ /proc/asgn1_info, /sys/class/...     │
   └───────────────┘                            └──────────────────────────────────────┘
```

| Thuộc tính | Giá trị |
|---|---|
| Module | `asgn1_driver.ko` |
| Device | `/dev/asgn1`, major **245** |
| procfs | `/proc/asgn1_info` |
| sysfs | `/sys/class/asgn1_class/asgn1/` |

## 📜 Bốn lệnh ioctl

| Lệnh | Macro | Mã hex | Dữ liệu | Hành vi |
|---|---|---|---|---|
| `ASGN1_RESET_BUFFER` | `_IO(0xA5, 0)` | `0x0000a500` | – | Xoá buffer, `write_count = read_count = 0` |
| `ASGN1_GET_STATS` | `_IOR(0xA5, 1, struct asgn1_stats)` | `0x8014a501` | 20 byte → user | 5 trường thống kê |
| `ASGN1_SET_MODE` | `_IOW(0xA5, 2, int)` | `0x4004a502` | 4 byte → kernel | 0 = KERNEL (thêm tiền tố), 1 = ECHO (mặc định); khác → `-EINVAL` |
| `ASGN1_GET_VERSION` | `_IOR(0xA5, 3, char[64])` | `0x8040a503` | 64 byte → user | Chuỗi version có MSSV |
| Lệnh khác | | | | `-ENOTTY` |

Giải mã `0x8014a501`:

```text
 0x8014a501 = 10 | 00000000010100 | 10100101 | 00000001
              DIR      SIZE           TYPE        NR
              2=READ   0x14 = 20      0xA5        1
                       = sizeof(struct asgn1_stats) (5 × int)
```

## ⚡ Build và chạy

```bash
cd ASSIGNMENT1/task1_ioctl
make                     # → asgn1_driver.ko + asgn1_app (ARM, statically linked)
../../tools/dev.sh       # build + đóng gói + boot QEMU
```

Trong QEMU:

```sh
test_asgn1.sh
```

Thử tay từng lệnh:

```sh
insmod /lib/modules/5.15.221/asgn1_driver.ko
asgn1_app version
echo hello > /dev/asgn1 ; cat /dev/asgn1          # hello
asgn1_app mode 0
echo hello > /dev/asgn1 ; cat /dev/asgn1          # [KERNEL] hello
asgn1_app stats                                   # buffer_len = 15, last_write_size = 6
asgn1_app mode 5                                  # ioctl SET_MODE: Invalid argument
asgn1_app invalid                                 # Inappropriate ioctl for device
asgn1_app reset ; cat /dev/asgn1                  # (rỗng)
dmesg | grep ioctl
```

Lưu log khi chạy test:

```bash
script -q -c "$HOME/embedded_lab2/dev.sh" ../logs/test_asgn1_output.txt
```

## ✅ Kết quả test

```text
[1] Load module ............................ 4 PASS
[2] Default mode (ECHO) .................... 1 PASS
[3] ASGN1_GET_VERSION ...................... 1 PASS
[4] SET_MODE 0 (KERNEL prefix) ............. 2 PASS
[5] ASGN1_GET_STATS ........................ 2 PASS
[6] SET_MODE 1 ............................. 2 PASS
[7] Error handling (-EINVAL, -ENOTTY) ...... 3 PASS
[8] Oversized write (2000 bytes) ........... 2 PASS
[9] RESET_BUFFER ........................... 4 PASS
[10] Unload module ......................... 3 PASS
 RESULT: 24 passed, 0 failed
```

Log đầy đủ: [`logs/test_asgn1_output.txt`](logs/test_asgn1_output.txt)

## 🧠 Quyết định thiết kế

| Quyết định | Lý do |
|---|---|
| Header `asgn1_ioctl.h` dùng chung, `#ifdef __KERNEL__` chọn include | Một nguồn duy nhất cho mã lệnh và struct → hai bên không thể lệch nhau |
| Kiểm tra `_IOC_TYPE(cmd) != ASGN1_MAGIC` trước `switch` | Loại ngay lệnh của driver khác |
| `GET_STATS`: chép vào struct cục bộ trong mutex → **mở khoá** → `copy_to_user` | `copy_to_user` có thể page-fault và ngủ; không giữ khoá trong lúc ngủ |
| `memset(&st, 0, …)` trước khi trả struct | Tránh lộ rác từ stack kernel ra userspace |
| `GET_VERSION`: `strscpy` vào buffer 64 byte, copy đúng 64 byte | Không bao giờ ghi quá buffer user; chuỗi luôn có `'\0'` |
| `read`/`write` viết theo mẫu `goto out` | Một chỗ `mutex_unlock` duy nhất → không thể quên khoá |
| `write` đọc `mode` **trong** mutex | `mode` có thể bị ioctl đổi bất cứ lúc nào |
| `memcpy` cho tiền tố, `copy_from_user` cho dữ liệu user | `memcpy` đúng khi cả hai đầu là bộ nhớ kernel |
| `last_write_size` = số byte user, `buffer_len` = gồm cả tiền tố | Tiền tố do driver thêm, không phải user ghi |
| `write` luôn trả về `len` kể cả khi phải cắt bớt | Trả ít hơn → shell gọi `write` lần nữa và **ghi đè** dữ liệu |
| `asgn1_app` trả exit code 1 khi lỗi | Script test phân biệt được "bị từ chối đúng" và "thành công ngoài ý muốn" |

## 🐞 Lỗi đã gặp và cách debug

**Ghi chuỗi dài bị mất phần đầu.** Ghi 2000 ký tự, `/proc/asgn1_info` chỉ còn 977 byte.

```text
asgn1_driver: write too large, truncating
asgn1_driver: received 1023 bytes (write #4)
asgn1_driver: received 977 bytes (write #5)      ← shell tự gọi write lần 2
Data length     : 977 bytes
```

Nguyên nhân: `write` cũ cắt `len` xuống 1023 rồi **trả về 1023** (< 2000). Userspace hiểu là ghi
được một phần → gọi lại với 977 byte còn lại → driver ghi đè từ đầu buffer.
Sửa: tính `copy_len` riêng, luôn trả về `len`. Sau khi sửa:

```text
asgn1_driver: write 2000 bytes truncated to 1023
asgn1_driver: received 2000 bytes, mode=1, stored 1023 bytes (write #4)
Data length     : 1023 bytes
```

**Đặt `'\0'` sai vị trí.** `device_buffer[buffer_len] = '\0'` đặt **trước** khi cập nhật
`buffer_len` → dùng độ dài của lần ghi trước → lọt dữ liệu cũ. Sửa: dùng độ dài mới.

---

# Task 2 – Misc device `counter_driver`

## 🏗️ Thiết kế

```text
 cat /dev/counter ──► counter_read  ── *ppos == 0 ? ──► val = atomic_fetch_inc(&counter)
                                                       chụp "val\n" vào private_data
                                     └─ trả bản chụp từ *ppos, hết thì trả 0 (EOF)
 echo N > /dev/counter ──► counter_write ──► kstrtoint ──► N < 0 hoặc không phải số → -EINVAL
                                                         ngược lại atomic_set(&counter, N)
 cat /proc/counter_info ──► giá trị hiện tại, tổng số lần đọc, tổng số lần ghi
```

| Thuộc tính | Giá trị |
|---|---|
| Module | `counter_driver.ko` |
| Đăng ký | `misc_register()` / `misc_deregister()` |
| Device | `/dev/counter`, major **10**, minor do kernel cấp, quyền 0666 |
| Trạng thái | `atomic_t counter, total_reads, total_writes` – không cần mutex |
| procfs | `/proc/counter_info` |

| Quyết định | Lý do |
|---|---|
| Dùng misc device thay vì `register_chrdev` | Mọi misc device dùng chung major 10; kernel tự cấp minor và tạo `/dev/counter`. Không cần class/device |
| `atomic_fetch_inc` (lấy **và** tăng trong một bước) | Tách `atomic_read` + `atomic_inc` thì hai tiến trình đọc cùng lúc có thể nhận cùng một giá trị |
| Bản chụp riêng cho từng lần mở trong `file->private_data` (cấp ở `open`, giải phóng ở `release`) | `cat` gọi `read` hai lần: lần 2 phải trả EOF và không tăng thêm; `dd bs=1` phải thấy một chuỗi nhất quán |
| Buffer kernel 16 byte có kiểm tra độ dài cho `write` | Dữ liệu user không bao giờ tràn bộ nhớ kernel; dài hơn → `-EINVAL` |
| Lần ghi bị từ chối không tính vào `total_writes` | Chỉ lần cập nhật thành công mới đổi thống kê |
| `init` gỡ misc device nếu `proc_create` lỗi | Huỷ các bước đã xong khi lỗi; `exit` làm theo thứ tự ngược |

## ⚡ Build và chạy

```bash
cd ASSIGNMENT1/task2_misc
../../tools/dev.sh
```

```sh
test_counter.sh
```

## ✅ Kết quả test

```text
[1] Nạp module (misc major 10, /proc/misc) ...... 5 PASS
[2] Đọc tự tăng 0 → 1 → 2 ....................... 3 PASS
[3] Thống kê trước khi ghi ...................... 2 PASS
[4] Ghi 100 → đọc 100, 101 ...................... 3 PASS
[5] Reset (ghi 0) ............................... 2 PASS
[6] Dữ liệu sai 'abc', '-5' → -EINVAL ........... 3 PASS
[7] dd bs=1: đủ giá trị, chỉ tăng một lần ....... 2 PASS
[8] Thống kê cuối (9 lần đọc, 2 lần ghi) ........ 2 PASS
[9] Gỡ module ................................... 3 PASS
 RESULT: 25 passed, 0 failed
```

`/proc/counter_info` ở 3 thời điểm đề yêu cầu (trích log):

```text
trước khi ghi   Current value : 3    Total reads : 3   Total writes : 0
sau khi ghi     Current value : 102  Total reads : 5   Total writes : 1
sau khi reset   Current value : 1    Total reads : 6   Total writes : 2
```

Log đầy đủ: [`logs/test_counter_output.txt`](logs/test_counter_output.txt)
