# 02 – Environment setup from scratch

**English** | [Tiếng Việt](vi/02-cai-dat-moi-truong.md) · [← Home](../README.md)

This guide rebuilds the **entire** environment on a fresh Ubuntu machine: toolchain, kernel,
BusyBox, rootfs and the `dev.sh` script. When it is done, every lab and assignment in this
repo can be built and run.

> ⏱️ Total time: about 45–60 minutes (mostly the kernel build).
> 🌐 Needs internet to download packages and sources. **Do it before exam day.**

---

## 0. Directory layout created by this guide

```text
~/embedded_lab1/
├── kernel/linux-5.15.221/      ← kernel source (KDIR of every driver Makefile)
├── rootfs/busybox-1.35.0/      ← BusyBox source
└── output/                     ← zImage, vexpress-v2p-ca9.dtb

~/embedded_lab2/
├── rootfs/initramfs/           ← working rootfs
├── output/initramfs_lab2.cpio.gz
└── dev.sh  → symlink to tools/dev.sh in this repo
```

The Makefiles and `dev.sh` in this repo **depend on these exact paths**. If you put things
elsewhere, update `KDIR` in every Makefile and the variables at the top of `tools/dev.sh`.

---

## 1. Install packages

```bash
sudo apt update
sudo apt install -y build-essential git bc bison flex libssl-dev libncurses-dev \
    gcc-arm-linux-gnueabihf qemu-system-arm \
    cpio gzip file device-tree-compiler u-boot-tools mtd-utils
```

Check:

```bash
arm-linux-gnueabihf-gcc --version | head -1
qemu-system-arm --version | head -1
```

---

## 2. Clone the repo

```bash
cd ~/Desktop
git clone git@github.com:Ygbzyem/Linux-and-Open-Source-Platform-Subject-Course-.git
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
```

---

## 3. Build kernel 5.15.221

> ❗ Use **5.15.221** (a recent stable patch release). The original 5.15.0 fails to build with
> GCC 15 because C23 makes `bool` a keyword.

```bash
mkdir -p ~/embedded_lab1/{kernel,rootfs,output}
cd ~/embedded_lab1/kernel
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.221.tar.xz
tar xf linux-5.15.221.tar.xz
cd linux-5.15.221
```

Reuse the config saved in this repo (recommended – identical to the tested kernel):

```bash
cp $REPO/LAB1/embedded_lab1/configs/kernel.config .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
```

Or start from the board default:

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- vexpress_defconfig
```

Build (replace `12` with your core count from `nproc`):

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12 zImage dtbs modules
cp arch/arm/boot/zImage                   ~/embedded_lab1/output/
cp arch/arm/boot/dts/vexpress-v2p-ca9.dtb ~/embedded_lab1/output/
```

> Do not add `KCFLAGS`. Build `modules` at least once – out-of-tree drivers need the kernel's
> `Module.symvers`.

**Checkpoint:** `ls -lh ~/embedded_lab1/output/` shows `zImage` (~5 MB) and `vexpress-v2p-ca9.dtb`.

---

## 4. Build BusyBox (static)

```bash
cd ~/embedded_lab1/rootfs
wget https://busybox.net/downloads/busybox-1.35.0.tar.bz2
tar xf busybox-1.35.0.tar.bz2
cd busybox-1.35.0
```

Two fixes required with GCC 15:

```bash
# 1) The menuconfig check script uses "main() {}" – newer GCC rejects it
sed -i 's/^main() {}/int main() { return 0; }/' scripts/kconfig/lxdialog/check-lxdialog.sh

# 2) Reuse the saved config (CONFIG_STATIC=y, CONFIG_TC disabled)
cp $REPO/LAB1/embedded_lab1/configs/busybox.config .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- oldconfig
```

> `CONFIG_TC` (traffic control) does not build against newer kernel headers → disable it.
> `CONFIG_STATIC=y` so BusyBox needs no shared libraries in the rootfs.

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- CONFIG_PREFIX=$HOME/embedded_lab2/rootfs/initramfs install
file $HOME/embedded_lab2/rootfs/initramfs/bin/busybox     # must say "statically linked"
```

---

## 5. Build the rootfs

```bash
R=~/embedded_lab2/rootfs/initramfs
mkdir -p ~/embedded_lab2/output
cd $R
mkdir -p proc sys dev tmp etc/init.d lib/modules/5.15.221 usr/bin
sudo mknod -m 600 dev/console c 5 1      # needed before devtmpfs is mounted
```

Copy the hand-written config files from the repo:

```bash
cp $REPO/LAB2/embedded_lab2/rootfs_custom/etc/inittab     $R/etc/inittab
cp $REPO/LAB2/embedded_lab2/rootfs_custom/etc/init.d/rcS  $R/etc/init.d/rcS
chmod +x $R/etc/init.d/rcS
```

The LAB-02 `rcS` mounts `proc`, `sysfs`, `tmpfs`, `devtmpfs`; sets the hostname; and
automatically runs `insmod lab2_driver.ko` if it exists in `lib/modules/5.15.221/`.

---

## 6. Install `dev.sh`

```bash
chmod +x $REPO/tools/dev.sh
ln -sf $REPO/tools/dev.sh ~/embedded_lab2/dev.sh
```

`dev.sh` runs four steps in a row, from a directory containing a driver Makefile:

```text
[2] make  →  [3] copy *.ko, *.sh, static ARM binaries into rootfs  →  [4] repack cpio  →  [5] boot QEMU
```

---

## 7. End-to-end check: build and run the LAB-02 driver

```bash
cd $REPO/LAB2/embedded_lab2/driver
~/embedded_lab2/dev.sh
```

Inside QEMU you should see:

```text
[ LAB-02 ] lab2_driver loaded OK
...
/ # test_driver.sh
=== Test PASSED ===
```

If this works, the environment is complete.

---

## 8. Extra tools for ASSIGNMENT-02 (install while online)

```bash
sudo apt install -y valgrind strace gdb linux-tools-common linux-tools-$(uname -r)
```

ASSIGNMENT-02 also needs two kernel options that are currently **off** in `kernel.config`:

| Option | Why |
|---|---|
| `CONFIG_POSIX_MQUEUE=y` | `mq_open()` for the alarm queue – otherwise `Function not implemented` |
| `CONFIG_HIGH_RES_TIMERS=y` | Sub-10 ms `clock_nanosleep` precision – needed for jitter measurements |

Neither option changes the vermagic, so existing modules still load after rebuilding.

---

**Next:** [03 – Exam runbook](03-exam-runbook.md)
