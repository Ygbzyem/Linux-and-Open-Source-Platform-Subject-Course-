# LAB-01 – Building a minimal Embedded Linux system

**English** | [Tiếng Việt](README.vi.md) · [← Home](../README.md)

## 🎯 Goal

Build all four components yourself (toolchain → kernel → bootloader → rootfs) and boot an ARM
system on QEMU all the way to a shell prompt.

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

Background: [docs/01 – What is Embedded Linux?](../docs/01-what-is-embedded-linux.md)

---

## 📁 Contents

| File | What it is |
|---|---|
| `embedded_lab1/configs/kernel.config` | Kernel 5.15.221 `.config` (from `vexpress_defconfig`) |
| `embedded_lab1/configs/busybox.config` | BusyBox `.config` – `CONFIG_STATIC=y`, `CONFIG_TC` off |
| `embedded_lab1/configs/uboot.config` | U-Boot `.config` for vexpress-ca9x4 |
| `embedded_lab1/rootfs_custom/etc/` | Hand-written `inittab`, `init.d/rcS`, `hostname`, `passwd` |
| `embedded_lab1/boot_log.txt` | Full kernel boot log up to the shell |
| `embedded_lab1/uboot_log.txt` | U-Boot log up to the `=>` prompt |
| `LAB01_EmbeddedLinux.docx` | Lab handout |

---

## 🛠️ Steps

Full copy-paste guide: [docs/02 – Environment setup](../docs/02-environment-setup.md).
Summary:

### 1. Kernel

```bash
cd ~/embedded_lab1/kernel/linux-5.15.221
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- vexpress_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12 zImage dtbs modules
```

Output: `arch/arm/boot/zImage`, `arch/arm/boot/dts/vexpress-v2p-ca9.dtb`.

### 2. U-Boot

```bash
cd ~/embedded_lab1/uboot/u-boot-2022.04
make CROSS_COMPILE=arm-linux-gnueabihf- vexpress_ca9x4_defconfig
make CROSS_COMPILE=arm-linux-gnueabihf- -j12
qemu-system-arm -M vexpress-a9 -m 512M -nographic -kernel u-boot
```

Result: stops at the `=>` prompt; `bdinfo` reports DRAM start `0x60000000`, size `0x20000000`
(512 MB).

### 3. BusyBox + rootfs

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- defconfig
# enable CONFIG_STATIC=y, disable CONFIG_TC (menuconfig, or use configs/busybox.config)
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

Pack:

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

## ✅ Results

| Item | Status |
|---|---|
| Kernel 5.15.221 build | ✅ |
| U-Boot 2022.04 build, reaches `=>` | ✅ |
| Static BusyBox build, rootfs | ✅ |
| Kernel + initramfs booted **directly by QEMU** to a shell | ✅ – see `boot_log.txt` |
| Kernel booted **through U-Boot** | ⏳ Not finished – see below |

From `boot_log.txt`:

```text
Run /sbin/init as init process
Boot completed! Welcome to Embedded Linux Lab 1
```

---

## 🧩 Problems encountered

| Problem | Cause | Solution |
|---|---|---|
| Kernel 5.15.0 fails on `bool` | GCC 15 defaults to C23 where `bool` is a keyword | Use patched **5.15.221** |
| BusyBox `menuconfig` claims ncurses is missing | `check-lxdialog.sh` uses `main() {}` – rejected by newer GCC | `sed -i 's/^main() {}/int main() { return 0; }/' scripts/kconfig/lxdialog/check-lxdialog.sh` |
| BusyBox fails building `tc` | Newer kernel headers drop old definitions | Disable `CONFIG_TC` |
| Booting via U-Boot: `Wrong Image Format for bootm` | `bootm` used for a `zImage` (needs `bootz`), and the DTB must be placed safely after U-Boot relocates itself to the top of RAM | Plan: load the DTB at `0x61000000`, `cp.b` it to a safe area, then `bootz` – avoids `FDT_ERR_BADSTRUCTURE` |

---

## ❓ Review questions

1. Why cross-compile? What does `arm-linux-gnueabihf` mean?
2. What problem does the Device Tree solve on ARM?
3. Which `inittab` actions exist and in what order do they run?
4. Why must BusyBox be built static?

Answers: [docs/07 – Viva Q&A](../docs/07-viva-qa.md), section A.
