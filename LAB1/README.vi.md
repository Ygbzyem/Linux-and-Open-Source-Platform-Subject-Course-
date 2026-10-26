# LAB-01 – Build một hệ thống Embedded Linux tối thiểu

[English](README.md) | **Tiếng Việt** · [← Về trang chủ](../README.vi.md)

## 🎯 Mục tiêu

Tự build đủ 4 thành phần (toolchain → kernel → bootloader → rootfs) và boot được hệ thống ARM
trên QEMU tới dấu nhắc shell.

```text
 arm-linux-gnueabihf-gcc ──build──► zImage + vexpress-v2p-ca9.dtb   (Linux 5.15.221)
                         ──build──► u-boot                          (U-Boot 2022.04)
                         ──build──► busybox (static) → initramfs    (BusyBox 1.35.0)
                                          │
                                          ▼
                             qemu-system-arm -M vexpress-a9 …
                                          │
                                          ▼
                     "Boot completed! Welcome to Embedded Linux Lab 1"
```

Kiến thức nền: [docs/01 – Embedded Linux là gì?](../docs/vi/01-embedded-linux-la-gi.md)

---

## 📁 Nội dung thư mục

| File | Là gì |
|---|---|
| `embedded_lab1/configs/kernel.config` | `.config` của kernel 5.15.221 (từ `vexpress_defconfig`) |
| `embedded_lab1/configs/busybox.config` | `.config` BusyBox – `CONFIG_STATIC=y`, `CONFIG_TC` tắt |
| `embedded_lab1/configs/uboot.config` | `.config` U-Boot cho vexpress-ca9x4 |
| `embedded_lab1/rootfs_custom/etc/` | `inittab`, `init.d/rcS`, `hostname`, `passwd` tự viết |
| `embedded_lab1/boot_log.txt` | Log boot kernel đầy đủ tới shell |
| `embedded_lab1/uboot_log.txt` | Log U-Boot tới dấu nhắc `=>` |
| `LAB01_EmbeddedLinux.docx` | Đề bài |

---

## 🛠️ Các bước

Hướng dẫn đầy đủ, copy-paste chạy được: [docs/02 – Cài đặt môi trường](../docs/vi/02-cai-dat-moi-truong.md).
Tóm tắt:

### 1. Kernel

```bash
cd ~/embedded_lab1/kernel/linux-5.15.221
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- vexpress_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12 zImage dtbs modules
```

Kết quả: `arch/arm/boot/zImage`, `arch/arm/boot/dts/vexpress-v2p-ca9.dtb`.

### 2. U-Boot

```bash
cd ~/embedded_lab1/uboot/u-boot-2022.04
make CROSS_COMPILE=arm-linux-gnueabihf- vexpress_ca9x4_defconfig
make CROSS_COMPILE=arm-linux-gnueabihf- -j12
qemu-system-arm -M vexpress-a9 -m 512M -nographic -kernel u-boot
```

Kết quả: dừng ở dấu nhắc `=>`; `bdinfo` báo DRAM bắt đầu `0x60000000`, kích thước
`0x20000000` (512 MB).

### 3. BusyBox + rootfs

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- defconfig
# bật CONFIG_STATIC=y, tắt CONFIG_TC (menuconfig hoặc dùng configs/busybox.config)
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- CONFIG_PREFIX=<rootfs> install
```

`inittab`:

```text
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/bin/umount -a -r
```

`rcS`:

```sh
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
hostname embedded-lab
echo 'Boot completed! Welcome to Embedded Linux Lab 1'
```

Đóng gói:

```bash
cd <rootfs> && find . | cpio -H newc -o | gzip > ~/embedded_lab1/output/initramfs.cpio.gz
```

### 4. Boot

```bash
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab1/output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init'
```

---

## ✅ Kết quả

| Hạng mục | Trạng thái |
|---|---|
| Build kernel 5.15.221 | ✅ |
| Build U-Boot 2022.04, chạy tới `=>` | ✅ |
| Build BusyBox static, dựng rootfs | ✅ |
| Boot kernel + initramfs **trực tiếp bằng QEMU** tới shell | ✅ – xem `boot_log.txt` |
| Boot kernel **thông qua U-Boot** | ⏳ Chưa xong – xem mục dưới |

Trích `boot_log.txt`:

```text
Run /sbin/init as init process
Boot completed! Welcome to Embedded Linux Lab 1
```

---

## 🧩 Khó khăn gặp phải

| Vấn đề | Nguyên nhân | Cách giải quyết |
|---|---|---|
| Kernel 5.15.0 build lỗi về `bool` | GCC 15 mặc định chuẩn C23, `bool` là từ khoá | Dùng bản vá **5.15.221** |
| BusyBox `menuconfig` báo thiếu ncurses dù đã cài | Script `check-lxdialog.sh` dùng `main() {}` – GCC mới từ chối | `sed -i 's/^main() {}/int main() { return 0; }/' scripts/kconfig/lxdialog/check-lxdialog.sh` |
| BusyBox lỗi khi build `tc` | Header kernel mới thiếu định nghĩa cũ | Tắt `CONFIG_TC` |
| Boot qua U-Boot: `Wrong Image Format for bootm` | Dùng `bootm` cho `zImage` (cần `bootz`), và cần chép DTB đúng chỗ sau khi U-Boot tự dời lên cuối RAM | Hướng xử lý: nạp DTB vào `0x61000000`, `cp.b` sang vùng an toàn, rồi `bootz` – tránh `FDT_ERR_BADSTRUCTURE` |

---

## ❓ Câu hỏi ôn tập

1. Vì sao phải cross-compile? `arm-linux-gnueabihf` nghĩa là gì?
2. Device Tree giải quyết vấn đề gì trên ARM?
3. `inittab` có những action nào, chạy theo thứ tự ra sao?
4. Vì sao BusyBox phải build static?

Gợi ý trả lời: [docs/07 – Vấn đáp](../docs/vi/07-van-dap.md), phần A.
