# LAB-02 – Character Device Driver, procfs/sysfs, MTD/JFFS2, BusyBox init

**English** | [Tiếng Việt](README.vi.md) · [← Home](../README.md)

## 🎯 Goals

| Part | Content | Points |
|---|---|---|
| A | Character driver `/dev/lab2`: open/read/write/release | 25 |
| B | Expose information via `/proc/lab2_info` and `/sys/class/lab2_class/lab2/` | 20 |
| C | Simulated MTD flash + JFFS2 filesystem (on the laptop) | 25 |
| D | Load the driver automatically at boot via `rcS` | 15 |

Line-by-line explanation of the code: [docs/04 – Driver anatomy](../docs/04-driver-anatomy.md).

---

## 📁 Contents

```text
embedded_lab2/
├── driver/
│   ├── lab2_driver.c        ← complete driver (Parts A + B) – BASE DRIVER for exercises/exam
│   ├── lab2_driver_partA.c  ← Part A only (no procfs/sysfs), kept for comparison
│   ├── Makefile
│   ├── test_driver.sh       ← Part A test
│   └── test_procfs.sh       ← Part B test
├── rootfs_custom/etc/       ← inittab, init.d/rcS (Part D)
├── logs/
│   ├── driver_test.txt      ← Part A
│   ├── procfs_test.txt      ← Part B
│   ├── mtd_jffs2.txt        ← Part C
│   └── boot_log.txt         ← Part D
└── repack.sh                ← repack the initramfs only (no build, no boot)
```

---

## ⚡ Re-run everything

```bash
cd LAB2/embedded_lab2/driver
../../../tools/dev.sh
```

Inside QEMU (the driver is already loaded by `rcS`):

```sh
test_driver.sh
test_procfs.sh
cat /proc/lab2_info
```

---

## 🅰️ Part A – Character driver

| Property | Value |
|---|---|
| Module | `lab2_driver.ko` |
| Device | `/dev/lab2`, major **240**, minor 0 |
| Buffer | 1024 bytes, mutex-protected |
| Registration | `register_chrdev` → `class_create` → `device_create` |

```sh
insmod /lib/modules/5.15.221/lab2_driver.ko
grep lab2 /proc/devices          # 240 lab2
echo 'Hello from userspace, LAB-02!' > /dev/lab2
cat /dev/lab2
rmmod lab2_driver
lsmod                            # lab2_driver gone
```

Log: [`logs/driver_test.txt`](embedded_lab2/logs/driver_test.txt)

---

## 🅱️ Part B – procfs & sysfs

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

| Observation | Explanation |
|---|---|
| `open_count` does not grow when reading `/proc/...` | `/proc` and `/sys` have their own handlers and never go through `lab2_fops.open` |
| sysfs files are `-r--r--r--` | `DEVICE_ATTR_RO` → 0444 |
| Extra `dev`, `uevent`, `power`, `subsystem` | Created by the kernel for every `struct device`; `cat dev` → `240:0` |

Log: [`logs/procfs_test.txt`](embedded_lab2/logs/procfs_test.txt)

---

## 🅲 Part C – MTD + JFFS2 (on the laptop)

> ⚠️ **On this laptop `mtd0` is the real BIOS flash chip.** Never hard-code `mtd0`.

```bash
sudo modprobe nandsim first_id_byte=0x20 second_id_byte=0x35   # 32 MiB NAND
sudo modprobe mtdblock
sudo modprobe jffs2

MTD=$(grep 'NAND simulator' /proc/mtd | head -1 | cut -d: -f1)
N=${MTD#mtd}
echo "$MTD"                    # must NOT be mtd0; empty → STOP

sudo mtdinfo /dev/$MTD         # 16 KiB erase block, 512 B page, 16 B OOB
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

**Persistence check:** write more data → `umount` → `mount` again → data is intact.

**Flash hexdump:**

```bash
sudo nanddump -o -l 512 -f /tmp/dump.bin /dev/$MTD     # newer mtd-utils require -f
hexdump -C /tmp/dump.bin | head
```

```text
00000000  85 19 01 e0 31 00 00 00 ...   ← 0x1985 magic, 0xE001 DIRENT
...                    68 65 6c 6c 6f 2e 74 78  |hello.tx|
00000030  74 ff ff ff 85 19 02 e0 ...   ← 0xE002 INODE
00000070  ...          48 65 6c 6c 6f 20 66 72  |Hello fr|
```

| `mkfs.jffs2` option | Meaning |
|---|---|
| `--eraseblock=0x4000` | 16 KiB – must match the erase size reported by `mtdinfo` |
| `--pad` | Pad the image to a whole erase block |
| `--no-cleanmarkers` | On NAND, cleanmarkers live in the OOB area and are written at run time |

Clean up: `sudo umount /mnt/jffs2; sudo rmmod jffs2 mtdblock nandsim`.

Log: [`logs/mtd_jffs2.txt`](embedded_lab2/logs/mtd_jffs2.txt)

---

## 🅳 Part D – Auto-load at boot

[`rootfs_custom/etc/init.d/rcS`](embedded_lab2/rootfs_custom/etc/init.d/rcS):

```sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
mount -t devtmpfs none /dev 2>/dev/null || true
hostname embedded-lab2
insmod /lib/modules/5.15.221/lab2_driver.ko
[ -c /dev/lab2 ] || mknod /dev/lab2 c 240 0      # devtmpfs usually created it already
chmod 666 /dev/lab2
```

Boot result:

```text
[ LAB-02 ] lab2_driver loaded OK
[ LAB-02 ] /dev/lab2 ready (major=240, minor=0)
Please press Enter to activate this console.
/ # hostname
embedded-lab2
/ # ls -la /dev/lab2
crw-rw-rw-    1 root     0         240,   0 … /dev/lab2
```

> 💡 Because `rcS` mounts `devtmpfs`, `device_create()` already created `/dev/lab2`. An
> unconditional `mknod` would fail with `File exists` – hence the `[ -c /dev/lab2 ]` check.

Log: [`logs/boot_log.txt`](embedded_lab2/logs/boot_log.txt)

---

## ✅ Results

| Part | Status |
|---|---|
| A – Character driver | ✅ |
| B – procfs/sysfs | ✅ |
| C – MTD/JFFS2, persistence across umount/mount | ✅ |
| D – Auto-load at boot | ✅ |

## 🔧 Known limitations

- `lab2_read` does not take the mutex → fixed in [ASSIGNMENT-01](../ASSIGNMENT1/README.md).
- The sysfs `*_show` functions read variables without the mutex.
