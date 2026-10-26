# 01 – Embedded Linux là gì?

[English](../01-what-is-embedded-linux.md) | **Tiếng Việt** · [← Về trang chủ](../../README.vi.md)

## 1. Một câu định nghĩa

**Embedded Linux** là dùng nhân Linux (và một bộ công cụ tối giản) làm hệ điều hành cho một
thiết bị chuyên dụng: router, camera IP, bộ điều khiển công nghiệp, TV, ô tô…

Khác với Ubuntu trên laptop:

| | Desktop Linux (Ubuntu) | Embedded Linux |
|---|---|---|
| Phần cứng | x86, RAM vài GB, ổ SSD | ARM/RISC-V, RAM vài chục MB, flash nhỏ |
| Ai build? | Canonical build sẵn | **Mình tự build** từng thành phần |
| Kích thước | Vài GB | Vài MB |
| Rootfs | Hàng nghìn package | BusyBox: 1 file thực thi làm ~300 lệnh |
| Biên dịch | Trên chính máy đó | **Cross-compile** trên máy khác |

So với vi điều khiển (ESP32, STM32 chạy FreeRTOS/bare-metal): Embedded Linux có MMU, có tiến
trình tách biệt bộ nhớ, có filesystem, có mạng, có driver chuẩn – đổi lại cần RAM/flash lớn hơn
và boot chậm hơn.

---

## 2. Bốn thành phần của mọi hệ thống Embedded Linux

```text
┌──────────────────────────────────────────────────────────────────┐
│ ④ ROOT FILESYSTEM   /bin /sbin /etc /dev /proc … (BusyBox)        │  ← userspace
├──────────────────────────────────────────────────────────────────┤
│ ③ LINUX KERNEL      zImage + driver + Device Tree (.dtb)          │  ← kernel space
├──────────────────────────────────────────────────────────────────┤
│ ② BOOTLOADER        U-Boot: khởi tạo RAM, nạp kernel vào RAM      │
├──────────────────────────────────────────────────────────────────┤
│   PHẦN CỨNG         ARM Cortex-A9 (ở đây: giả lập bằng QEMU)       │
└──────────────────────────────────────────────────────────────────┘
  ① TOOLCHAIN  arm-linux-gnueabihf-gcc – nằm trên máy host, dùng để build ②③④
```

| # | Thành phần | Vai trò | Trong repo này |
|---|---|---|---|
| ① | **Toolchain** | Biên dịch code trên x86 ra mã ARM | `arm-linux-gnueabihf-` (GCC 15.2) |
| ② | **Bootloader** | Chạy đầu tiên khi cấp điện, nạp kernel | U-Boot 2022.04 |
| ③ | **Kernel** | Quản lý CPU, bộ nhớ, thiết bị, tiến trình | Linux 5.15.221 |
| ④ | **Rootfs** | Chương trình và file cấu hình người dùng thấy | BusyBox 1.35.0 + initramfs |

### Cross-compile là gì?

Laptop là x86, board là ARM. Một file biên dịch bằng `gcc` thường sẽ **không chạy** được trên
ARM. Vì vậy ta dùng `arm-linux-gnueabihf-gcc` – compiler chạy trên x86 nhưng sinh mã ARM.

```bash
file asgn1_app
# ELF 32-bit LSB executable, ARM, EABI5 ... statically linked
```

- `arm-linux-gnueabihf` = kiến trúc **arm**, hệ điều hành **linux**, ABI **gnueabi** với
  **hf** (hard-float: dùng FPU phần cứng để tính số thực).
- Với kernel, biến `ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-` báo cho Makefile của kernel
  dùng đúng compiler này.

---

## 3. Hệ thống boot như thế nào?

### 3.1. Trên board thật

```text
 Cấp điện
    │
    ▼
 ROM code trong SoC ──► nạp U-Boot từ flash/SD
    │
    ▼
 U-Boot  ──► khởi tạo RAM, đọc zImage + .dtb + rootfs vào RAM
    │        bootz <địa chỉ kernel> <địa chỉ initrd> <địa chỉ dtb>
    ▼
 Kernel  ──► giải nén chính nó, đọc Device Tree để biết phần cứng,
    │        khởi tạo driver, mount rootfs
    ▼
 /sbin/init (BusyBox) ──► đọc /etc/inittab
    │
    ▼
 ::sysinit:/etc/init.d/rcS   ──► mount /proc /sys /dev, insmod driver…
    │
    ▼
 ::askfirst:-/bin/sh         ──► "Please press Enter to activate this console."
    │
    ▼
 / #   ← shell, hệ thống sẵn sàng
```

### 3.2. Trong repo này (QEMU nạp thẳng kernel)

QEMU đóng vai cả phần cứng lẫn bootloader: nó tự nạp `zImage`, `.dtb` và initramfs vào RAM
rồi nhảy vào kernel. U-Boot đã build ở LAB-01 nhưng không nằm trong đường boot hằng ngày.

```bash
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb    ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init'
```

| Tham số | Ý nghĩa |
|---|---|
| `-M vexpress-a9` | Giả lập board ARM Versatile Express với CPU Cortex-A9 |
| `-m 512M -smp 2` | 512 MB RAM, 2 CPU |
| `-nographic` | Không mở cửa sổ; cổng serial nối thẳng vào terminal |
| `-kernel` | File kernel nén |
| `-dtb` | Device Tree: mô tả phần cứng cho kernel |
| `-initrd` | Rootfs đóng gói dạng cpio nén |
| `console=ttyAMA0,115200` | In log kernel ra UART đầu tiên (chính là terminal) |
| `rdinit=/sbin/init` | Chương trình đầu tiên chạy trong initramfs |

Thoát QEMU: **Ctrl+A rồi X**.

---

## 4. Device Tree – vì sao kernel cần file `.dtb`?

Trên PC, phần cứng tự khai báo (PCI, ACPI). Trên ARM, phần lớn **không tự khai báo được**:
kernel không tự biết UART nằm ở địa chỉ nào, có mấy CPU, RAM bắt đầu ở đâu.

Device Tree là file mô tả phần cứng dạng cây:

```text
/ {
    memory@60000000 { reg = <0x60000000 0x20000000>; };   ← RAM 512 MB từ 0x60000000
    uart@9000 { compatible = "arm,pl011"; … };             ← UART PL011
};
```

Kernel đọc `compatible = "arm,pl011"` và tìm driver khai báo cùng chuỗi đó. Nhờ vậy **một**
`zImage` có thể chạy trên nhiều board, chỉ cần đổi file `.dtb`.

---

## 5. initramfs – rootfs nằm trong RAM

```text
~/embedded_lab2/rootfs/initramfs/      (thư mục trên laptop)
        │   find . | cpio -H newc -o | gzip
        ▼
initramfs_lab2.cpio.gz                 (1 file nén)
        │   QEMU nạp vào RAM (-initrd)
        ▼
Kernel giải nén → thành filesystem gốc "/"
```

Hệ quả quan trọng:

- **Sửa gì trong thư mục rootfs trên laptop cũng phải đóng gói lại** thì QEMU mới thấy.
  Quên bước này là nguyên nhân số 1 của lỗi `not found` trong QEMU.
- Mọi thay đổi trong QEMU (tạo file, ghi log) **mất hết khi tắt máy**, vì nó chỉ nằm trong RAM.

---

## 6. BusyBox – một file, hàng trăm lệnh

`/bin/busybox` là **một file thực thi duy nhất**. `ls`, `cat`, `mount`, `insmod`… đều là
symlink trỏ tới nó; BusyBox nhìn tên lệnh được gọi (`argv[0]`) để biết phải làm gì.

```sh
/ # ls -l /bin/ls
lrwxrwxrwx ... /bin/ls -> busybox
```

Rootfs được build **static** (`CONFIG_STATIC=y`): mọi thư viện được nhúng thẳng vào file thực
thi, nên rootfs không cần `/lib/libc.so`. Cũng vì vậy, **mọi chương trình C tự viết cũng phải
build với `-static`**, nếu không sẽ báo `not found` dù file có tồn tại.

---

## 7. Kernel space vs User space

```text
  USER SPACE   echo, cat, asgn1_app …     – mỗi tiến trình có bộ nhớ ảo riêng
  ───────────────── system call (open/read/write/ioctl) ─────────────────
  KERNEL SPACE VFS → driver của mình (lab2_driver.ko, asgn1_driver.ko)
  ─────────────────────────────────────────────────────────────────────────
  PHẦN CỨNG
```

- Chương trình user **không truy cập phần cứng trực tiếp**; nó gọi system call.
- Kernel module (`.ko`) là code chạy **trong kernel**, nạp/gỡ lúc đang chạy bằng
  `insmod` / `rmmod`. Lỗi trong module có thể làm treo cả hệ thống – không có "crash riêng"
  như chương trình thường.
- Dữ liệu qua lại giữa hai vùng phải dùng `copy_to_user` / `copy_from_user`
  (giải thích ở [docs/07-van-dap.md](07-van-dap.md)).

---

## 8. Bản đồ các file `/dev`, `/proc`, `/sys`

| Đường dẫn | Là gì | Ví dụ trong các lab |
|---|---|---|
| `/dev/…` | File thiết bị – cửa để userspace nói chuyện với driver | `/dev/lab2`, `/dev/asgn1` |
| `/proc/…` | Thông tin kernel/driver dạng text, do driver sinh ra khi đọc | `/proc/lab2_info`, `/proc/devices` |
| `/sys/…` | Cây thiết bị của kernel, mỗi thuộc tính một file | `/sys/class/lab2_class/lab2/buffer_len` |
| `/proc/modules` | Danh sách module đang nạp (lệnh `lsmod` đọc file này) | |
| `/proc/mtd` | Danh sách thiết bị flash MTD | LAB-02 Phần C |

---

**Đọc tiếp:** [02 – Cài đặt môi trường](02-cai-dat-moi-truong.md)
