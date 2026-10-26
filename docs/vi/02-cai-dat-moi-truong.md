# 02 – Cài đặt môi trường từ đầu

[English](../02-environment-setup.md) | **Tiếng Việt** · [← Về trang chủ](../../README.vi.md)

Tài liệu này dựng lại **toàn bộ** môi trường trên một máy Ubuntu mới: toolchain, kernel,
BusyBox, rootfs và script `dev.sh`. Làm xong là chạy được mọi lab và assignment trong repo.

> ⏱️ Tổng thời gian: khoảng 45–60 phút (build kernel chiếm phần lớn).
> 🌐 Cần internet để tải gói và source. **Hãy làm trước ngày thi.**

---

## 0. Sơ đồ thư mục sẽ tạo ra

```text
~/embedded_lab1/
├── kernel/linux-5.15.221/      ← source kernel (KDIR của mọi Makefile driver)
├── rootfs/busybox-1.35.0/      ← source BusyBox
└── output/                     ← zImage, vexpress-v2p-ca9.dtb

~/embedded_lab2/
├── rootfs/initramfs/           ← rootfs đang dùng
├── output/initramfs_lab2.cpio.gz
└── dev.sh  → symlink tới tools/dev.sh trong repo
```

Các Makefile và `dev.sh` trong repo **dựa vào đúng các đường dẫn này**. Nếu đặt chỗ khác,
phải sửa `KDIR` trong mọi Makefile và các biến đầu `tools/dev.sh`.

---

## 1. Cài gói cần thiết

```bash
sudo apt update
sudo apt install -y build-essential git bc bison flex libssl-dev libncurses-dev \
    gcc-arm-linux-gnueabihf qemu-system-arm \
    cpio gzip file device-tree-compiler u-boot-tools mtd-utils
```

Kiểm tra:

```bash
arm-linux-gnueabihf-gcc --version | head -1
qemu-system-arm --version | head -1
```

---

## 2. Clone repo

```bash
cd ~/Desktop
git clone git@github.com:Ygbzyem/Linux-and-Open-Source-Platform-Subject-Course-.git
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
```

---

## 3. Build kernel 5.15.221

> ❗ Phải dùng bản **5.15.221** (bản vá mới). Bản 5.15.0 gốc build lỗi với GCC 15 vì chuẩn C23
> coi `bool` là từ khoá.

```bash
mkdir -p ~/embedded_lab1/{kernel,rootfs,output}
cd ~/embedded_lab1/kernel
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.221.tar.xz
tar xf linux-5.15.221.tar.xz
cd linux-5.15.221
```

Dùng lại file cấu hình đã lưu trong repo (khuyên dùng – giống hệt kernel đã test):

```bash
cp $REPO/LAB1/embedded_lab1/configs/kernel.config .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
```

Hoặc bắt đầu từ cấu hình mặc định của board:

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- vexpress_defconfig
```

Build (thay `12` bằng số core của máy, xem bằng `nproc`):

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12 zImage dtbs modules
cp arch/arm/boot/zImage                 ~/embedded_lab1/output/
cp arch/arm/boot/dts/vexpress-v2p-ca9.dtb ~/embedded_lab1/output/
```

> Không thêm `KCFLAGS` vào lệnh build. `modules` phải được build ít nhất một lần, vì
> driver ngoài cây (out-of-tree) cần file `Module.symvers` của kernel.

**Checkpoint:** `ls -lh ~/embedded_lab1/output/` có `zImage` (~5 MB) và `vexpress-v2p-ca9.dtb`.

---

## 4. Build BusyBox (static)

```bash
cd ~/embedded_lab1/rootfs
wget https://busybox.net/downloads/busybox-1.35.0.tar.bz2
tar xf busybox-1.35.0.tar.bz2
cd busybox-1.35.0
```

Hai sửa lỗi bắt buộc với GCC 15:

```bash
# 1) Script kiểm tra menuconfig dùng "main() {}" – GCC mới từ chối
sed -i 's/^main() {}/int main() { return 0; }/' scripts/kconfig/lxdialog/check-lxdialog.sh

# 2) Dùng lại config đã lưu (đã bật CONFIG_STATIC=y và tắt CONFIG_TC)
cp $REPO/LAB1/embedded_lab1/configs/busybox.config .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- oldconfig
```

> `CONFIG_TC` (lệnh traffic control) không build được với header kernel mới → phải tắt.
> `CONFIG_STATIC=y` để BusyBox không cần thư viện động trong rootfs.

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- CONFIG_PREFIX=$HOME/embedded_lab2/rootfs/initramfs install
file $HOME/embedded_lab2/rootfs/initramfs/bin/busybox     # phải có "statically linked"
```

---

## 5. Dựng rootfs

```bash
R=~/embedded_lab2/rootfs/initramfs
mkdir -p ~/embedded_lab2/output
cd $R
mkdir -p proc sys dev tmp etc/init.d lib/modules/5.15.221 usr/bin
sudo mknod -m 600 dev/console c 5 1      # cần trước khi devtmpfs được mount
```

Chép các file cấu hình tự viết từ repo:

```bash
cp $REPO/LAB2/embedded_lab2/rootfs_custom/etc/inittab        $R/etc/inittab
cp $REPO/LAB2/embedded_lab2/rootfs_custom/etc/init.d/rcS     $R/etc/init.d/rcS
chmod +x $R/etc/init.d/rcS
```

`rcS` của LAB-02 sẽ: mount `proc`, `sysfs`, `tmpfs`, `devtmpfs`; đặt hostname; và tự
`insmod lab2_driver.ko` nếu file có trong `lib/modules/5.15.221/`.

---

## 6. Cài `dev.sh`

```bash
chmod +x $REPO/tools/dev.sh
ln -sf $REPO/tools/dev.sh ~/embedded_lab2/dev.sh
```

`dev.sh` thực hiện 4 bước liên tiếp, chạy trong thư mục có Makefile của driver:

```text
[2] make  →  [3] chép *.ko, *.sh, binary ARM tĩnh vào rootfs  →  [4] đóng gói cpio  →  [5] boot QEMU
```

---

## 7. Kiểm tra toàn bộ: build và chạy driver LAB-02

```bash
cd $REPO/LAB2/embedded_lab2/driver
~/embedded_lab2/dev.sh
```

Trong QEMU phải thấy:

```text
[ LAB-02 ] lab2_driver loaded OK
...
/ # test_driver.sh
=== Test PASSED ===
```

Đạt bước này là môi trường đã hoàn chỉnh.

---

## 8. Công cụ thêm cho ASSIGNMENT-02 (cài khi còn mạng)

```bash
sudo apt install -y valgrind strace gdb linux-tools-common linux-tools-$(uname -r)
```

ASSIGNMENT-02 cần thêm 2 option kernel (hiện đang tắt trong `kernel.config`):

| Option | Vì sao cần |
|---|---|
| `CONFIG_POSIX_MQUEUE=y` | `mq_open()` cho alarm queue – thiếu sẽ báo `Function not implemented` |
| `CONFIG_HIGH_RES_TIMERS=y` | `clock_nanosleep` chính xác dưới 10 ms – cần để đo jitter |

Hai option này không đổi vermagic, nên các module cũ vẫn `insmod` được sau khi build lại.

---

**Đọc tiếp:** [03 – Quy trình làm bài thi](03-quy-trinh-thi.md)
