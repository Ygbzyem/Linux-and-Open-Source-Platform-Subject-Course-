# LAB-02 – Character Device Driver, procfs/sysfs, MTD/JFFS2, BusyBox init

[English](README.md) | **Tiếng Việt** · [← Về trang chủ](../README.vi.md)

## 🎯 Mục tiêu

| Phần | Nội dung | Điểm |
|---|---|---|
| A | Character driver `/dev/lab2`: open/read/write/release | 25 |
| B | Xuất thông tin qua `/proc/lab2_info` và `/sys/class/lab2_class/lab2/` | 20 |
| C | Flash MTD giả lập + filesystem JFFS2 (chạy trên laptop) | 25 |
| D | Tự nạp driver khi boot qua `rcS` | 15 |

Giải thích chi tiết từng dòng code: [docs/04 – Giải phẫu driver](../docs/vi/04-giai-phau-driver.md).

---

## 📁 Nội dung thư mục

```text
embedded_lab2/
├── driver/
│   ├── lab2_driver.c        ← bản hoàn chỉnh (Phần A + B) – DRIVER GỐC cho bài luyện/thi
│   ├── lab2_driver_partA.c  ← bản chỉ có Phần A (chưa có procfs/sysfs), giữ để so sánh
│   ├── Makefile
│   ├── test_driver.sh       ← test Phần A
│   └── test_procfs.sh       ← test Phần B
├── rootfs_custom/etc/       ← inittab, init.d/rcS (Phần D)
├── logs/
│   ├── driver_test.txt      ← Phần A
│   ├── procfs_test.txt      ← Phần B
│   ├── mtd_jffs2.txt        ← Phần C
│   └── boot_log.txt         ← Phần D
└── repack.sh                ← chỉ đóng gói lại initramfs (không build, không boot)
```

---

## ⚡ Chạy lại toàn bộ

```bash
cd LAB2/embedded_lab2/driver
../../../tools/dev.sh
```

Trong QEMU (driver đã được `rcS` tự nạp):

```sh
test_driver.sh
test_procfs.sh
cat /proc/lab2_info
```

---

## 🅰️ Phần A – Character driver

| Thuộc tính | Giá trị |
|---|---|
| Module | `lab2_driver.ko` |
| Device | `/dev/lab2`, major **240**, minor 0 |
| Buffer | 1024 byte, bảo vệ bằng mutex |
| Đăng ký | `register_chrdev` → `class_create` → `device_create` |

```sh
insmod /lib/modules/5.15.221/lab2_driver.ko
grep lab2 /proc/devices          # 240 lab2
echo 'Hello from userspace, LAB-02!' > /dev/lab2
cat /dev/lab2
rmmod lab2_driver
lsmod                            # không còn lab2_driver
```

Log: [`logs/driver_test.txt`](embedded_lab2/logs/driver_test.txt)

---

## 🅱️ Phần B – procfs & sysfs

```sh
cat /proc/lab2_info
ls -l /sys/class/lab2_class/lab2/
cat /sys/class/lab2_class/lab2/buffer_len
cat /sys/class/lab2_class/lab2/open_count
cat /sys/class/lab2_class/lab2/last_data
```

```text
LAB-02 Driver Statistics
======================
Driver name   : lab2
Major number  : 240
Buffer size   : 1024 bytes
Data length   : 30 bytes
Open count    : 1
Last data     : [Embedded Linux Lab2 Test Data
]
```

| Quan sát | Giải thích |
|---|---|
| `open_count` không tăng khi `cat /proc/...` | `/proc` và `/sys` có hàm riêng, không đi qua `lab2_fops.open` |
| Các file sysfs có quyền `-r--r--r--` | `DEVICE_ATTR_RO` → 0444 |
| Có thêm `dev`, `uevent`, `power`, `subsystem` | Kernel tự tạo cho mọi `struct device`; `cat dev` → `240:0` |

Log: [`logs/procfs_test.txt`](embedded_lab2/logs/procfs_test.txt)

---

## 🅲 Phần C – MTD + JFFS2 (chạy trên laptop)

> ⚠️ **Laptop có `mtd0` là chip flash BIOS thật.** Không bao giờ gõ cứng `mtd0`.

```bash
sudo modprobe nandsim first_id_byte=0x20 second_id_byte=0x35   # NAND 32 MiB
sudo modprobe mtdblock
sudo modprobe jffs2

MTD=$(grep 'NAND simulator' /proc/mtd | head -1 | cut -d: -f1)
N=${MTD#mtd}
echo "$MTD"                    # phải KHÁC mtd0; rỗng → DỪNG

sudo mtdinfo /dev/$MTD         # erase block 16 KiB, page 512 B, OOB 16 B
sudo flash_erase /dev/$MTD 0 0

mkdir -p /tmp/jffs2_data/logs
echo 'Hello from JFFS2!'   > /tmp/jffs2_data/hello.txt
echo 'Embedded Linux Lab2' > /tmp/jffs2_data/info.txt
date                       > /tmp/jffs2_data/logs/boot.log
sudo mkfs.jffs2 --root=/tmp/jffs2_data --eraseblock=0x4000 \
     --output=/tmp/rootfs.jffs2 --pad --no-cleanmarkers

sudo nandwrite -p /dev/$MTD /tmp/rootfs.jffs2
sudo mkdir -p /mnt/jffs2
sudo mount -t jffs2 /dev/mtdblock$N /mnt/jffs2
```

**Kiểm tra persistence:** ghi thêm → `umount` → `mount` lại → dữ liệu còn nguyên.

**Hexdump flash:**

```bash
sudo nanddump -o -l 512 -f /tmp/dump.bin /dev/$MTD     # bản mtd-utils mới bắt buộc -f
hexdump -C /tmp/dump.bin | head
```

```text
00000000  85 19 01 e0 31 00 00 00 ...   ← 0x1985 magic, 0xE001 DIRENT
...                    68 65 6c 6c 6f 2e 74 78  |hello.tx|
00000030  74 ff ff ff 85 19 02 e0 ...   ← 0xE002 INODE
00000070  ...          48 65 6c 6c 6f 20 66 72  |Hello fr|
```

| Tham số `mkfs.jffs2` | Ý nghĩa |
|---|---|
| `--eraseblock=0x4000` | 16 KiB – phải khớp erase size của `mtdinfo` |
| `--pad` | Đệm ảnh cho tròn erase block |
| `--no-cleanmarkers` | Trên NAND, cleanmarker ghi vào OOB lúc chạy, không nhúng vào ảnh |

Dọn dẹp: `sudo umount /mnt/jffs2; sudo rmmod jffs2 mtdblock nandsim`.

Log: [`logs/mtd_jffs2.txt`](embedded_lab2/logs/mtd_jffs2.txt)

---

## 🅳 Phần D – Tự nạp driver khi boot

[`rootfs_custom/etc/init.d/rcS`](embedded_lab2/rootfs_custom/etc/init.d/rcS):

```sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
mount -t devtmpfs none /dev 2>/dev/null || true
hostname embedded-lab2
insmod /lib/modules/5.15.221/lab2_driver.ko
[ -c /dev/lab2 ] || mknod /dev/lab2 c 240 0      # devtmpfs thường đã tạo sẵn
chmod 666 /dev/lab2
```

Kết quả boot:

```text
[ LAB-02 ] lab2_driver loaded OK
[ LAB-02 ] /dev/lab2 ready (major=240, minor=0)
Please press Enter to activate this console.
/ # hostname
embedded-lab2
/ # ls -la /dev/lab2
crw-rw-rw-    1 root     0         240,   0 … /dev/lab2
```

> 💡 Vì `rcS` dùng `devtmpfs`, `device_create()` đã tự tạo `/dev/lab2`. `mknod` không điều kiện
> sẽ báo `File exists` – nên phải kiểm tra `[ -c /dev/lab2 ]` trước.

Log: [`logs/boot_log.txt`](embedded_lab2/logs/boot_log.txt)

---

## ✅ Kết quả

| Phần | Trạng thái |
|---|---|
| A – Character driver | ✅ |
| B – procfs/sysfs | ✅ |
| C – MTD/JFFS2, persistence sau umount/mount | ✅ |
| D – Tự nạp khi boot | ✅ |

## 🔧 Điểm còn có thể cải thiện

- `lab2_read` chưa khoá mutex → đã sửa trong [ASSIGNMENT-01](../ASSIGNMENT1/README.vi.md).
- Các hàm sysfs `*_show` đọc biến không qua mutex.
