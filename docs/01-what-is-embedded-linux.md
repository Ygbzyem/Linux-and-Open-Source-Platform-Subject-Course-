# 01 – What is Embedded Linux?

**English** | [Tiếng Việt](vi/01-embedded-linux-la-gi.md) · [← Home](../README.md)

## 1. One-sentence definition

**Embedded Linux** means using the Linux kernel (plus a minimal set of tools) as the operating
system of a dedicated device: routers, IP cameras, industrial controllers, TVs, cars…

Compared with Ubuntu on a laptop:

| | Desktop Linux (Ubuntu) | Embedded Linux |
|---|---|---|
| Hardware | x86, GBs of RAM, SSD | ARM/RISC-V, tens of MB of RAM, small flash |
| Who builds it? | Canonical ships it pre-built | **You build** every component |
| Size | Several GB | A few MB |
| Rootfs | Thousands of packages | BusyBox: one binary acting as ~300 commands |
| Compilation | On the machine itself | **Cross-compiled** on another machine |

Compared with a microcontroller (ESP32, STM32 running FreeRTOS/bare-metal): Embedded Linux has
an MMU, memory-isolated processes, filesystems, networking and standard drivers – at the cost
of more RAM/flash and a slower boot.

---

## 2. The four components of every Embedded Linux system

```text
┌──────────────────────────────────────────────────────────────────┐
│ ④ ROOT FILESYSTEM   /bin /sbin /etc /dev /proc … (BusyBox)        │  ← user space
├──────────────────────────────────────────────────────────────────┤
│ ③ LINUX KERNEL      zImage + drivers + Device Tree (.dtb)         │  ← kernel space
├──────────────────────────────────────────────────────────────────┤
│ ② BOOTLOADER        U-Boot: initialises RAM, loads the kernel     │
├──────────────────────────────────────────────────────────────────┤
│   HARDWARE          ARM Cortex-A9 (here: emulated by QEMU)         │
└──────────────────────────────────────────────────────────────────┘
  ① TOOLCHAIN  arm-linux-gnueabihf-gcc – lives on the host, builds ②③④
```

| # | Component | Role | In this repo |
|---|---|---|---|
| ① | **Toolchain** | Compiles on x86, produces ARM code | `arm-linux-gnueabihf-` (GCC 15.2) |
| ② | **Bootloader** | Runs first at power-on, loads the kernel | U-Boot 2022.04 |
| ③ | **Kernel** | Manages CPU, memory, devices, processes | Linux 5.15.221 |
| ④ | **Rootfs** | Programs and config files the user sees | BusyBox 1.35.0 + initramfs |

### What is cross-compiling?

The laptop is x86, the board is ARM. A binary built with plain `gcc` will **not run** on ARM.
We use `arm-linux-gnueabihf-gcc` – a compiler that runs on x86 but emits ARM code.

```bash
file asgn1_app
# ELF 32-bit LSB executable, ARM, EABI5 ... statically linked
```

- `arm-linux-gnueabihf` = architecture **arm**, OS **linux**, ABI **gnueabi** with **hf**
  (hard-float: floating point is done by the hardware FPU).
- For the kernel, `ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-` tells the kernel build system
  to use that compiler.

---

## 3. How does the system boot?

### 3.1. On a real board

```text
 Power on
    │
    ▼
 SoC ROM code ──► loads U-Boot from flash/SD
    │
    ▼
 U-Boot  ──► initialises RAM, reads zImage + .dtb + rootfs into RAM
    │        bootz <kernel addr> <initrd addr> <dtb addr>
    ▼
 Kernel  ──► decompresses itself, reads the Device Tree to learn the hardware,
    │        initialises drivers, mounts the rootfs
    ▼
 /sbin/init (BusyBox) ──► reads /etc/inittab
    │
    ▼
 ::sysinit:/etc/init.d/rcS   ──► mount /proc /sys /dev, insmod drivers…
    │
    ▼
 ::askfirst:-/bin/sh         ──► "Please press Enter to activate this console."
    │
    ▼
 / #   ← shell, system ready
```

### 3.2. In this repo (QEMU loads the kernel directly)

QEMU plays both hardware and bootloader: it loads `zImage`, the `.dtb` and the initramfs into
RAM itself and jumps into the kernel. U-Boot was built in LAB-01 but is not part of the
everyday boot path.

```bash
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb    ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init'
```

| Option | Meaning |
|---|---|
| `-M vexpress-a9` | Emulate an ARM Versatile Express board with a Cortex-A9 |
| `-m 512M -smp 2` | 512 MB RAM, 2 CPUs |
| `-nographic` | No window; the serial port is wired to the terminal |
| `-kernel` | Compressed kernel image |
| `-dtb` | Device Tree describing the hardware |
| `-initrd` | Rootfs packed as a compressed cpio archive |
| `console=ttyAMA0,115200` | Kernel console on the first UART (= the terminal) |
| `rdinit=/sbin/init` | First program to run from the initramfs |

Quit QEMU: **Ctrl+A, then X**.

---

## 4. Device Tree – why does the kernel need a `.dtb`?

On a PC, hardware describes itself (PCI, ACPI). On ARM, most of it **cannot**: the kernel has
no way to discover which address the UART lives at, how many CPUs there are, or where RAM
starts.

The Device Tree is a tree-shaped description of the hardware (simplified example):

```text
/ {
    memory@60000000 { reg = <0x60000000 0x20000000>; };   ← 512 MB RAM at 0x60000000
    uart@9000 { compatible = "arm,pl011"; … };             ← PL011 UART
};
```

The kernel reads `compatible = "arm,pl011"` and finds the driver declaring the same string.
That is how **one** `zImage` can run on many boards – only the `.dtb` changes.

---

## 5. initramfs – a root filesystem in RAM

```text
~/embedded_lab2/rootfs/initramfs/      (directory on the laptop)
        │   find . | cpio -H newc -o | gzip
        ▼
initramfs_lab2.cpio.gz                 (one compressed file)
        │   QEMU loads it into RAM (-initrd)
        ▼
Kernel unpacks it → becomes the root filesystem "/"
```

Important consequences:

- **Any change in the rootfs directory on the laptop must be repacked** before QEMU sees it.
  Forgetting this is the #1 cause of `not found` errors inside QEMU.
- Everything changed inside QEMU (new files, logs) **is lost at power-off** – it only lives in RAM.

---

## 6. BusyBox – one binary, hundreds of commands

`/bin/busybox` is **a single executable**. `ls`, `cat`, `mount`, `insmod`… are all symlinks
to it; BusyBox looks at the name it was invoked with (`argv[0]`) to decide what to do.

```sh
/ # ls -l /bin/ls
lrwxrwxrwx ... /bin/ls -> busybox
```

The rootfs is built **static** (`CONFIG_STATIC=y`): every library is linked into the
executable, so the rootfs has no `/lib/libc.so`. For the same reason, **every C program you
write must be built with `-static`**, otherwise it fails with `not found` even though the file
exists.

---

## 7. Kernel space vs user space

```text
  USER SPACE   echo, cat, asgn1_app …     – each process has its own virtual memory
  ───────────────── system calls (open/read/write/ioctl) ─────────────────
  KERNEL SPACE VFS → our drivers (lab2_driver.ko, asgn1_driver.ko)
  ─────────────────────────────────────────────────────────────────────────
  HARDWARE
```

- User programs **never touch hardware directly**; they make system calls.
- A kernel module (`.ko`) is code that runs **inside the kernel**, loaded/unloaded at run time
  with `insmod` / `rmmod`. A bug in a module can bring down the whole system – there is no
  "it only crashed itself" like a normal program.
- Data between the two worlds must go through `copy_to_user` / `copy_from_user`
  (explained in [docs/07-viva-qa.md](07-viva-qa.md)).

---

## 8. Map of `/dev`, `/proc`, `/sys`

| Path | What it is | Examples from the labs |
|---|---|---|
| `/dev/…` | Device files – the door from user space to a driver | `/dev/lab2`, `/dev/asgn1` |
| `/proc/…` | Kernel/driver info as text, generated on read | `/proc/lab2_info`, `/proc/devices` |
| `/sys/…` | The kernel's device tree, one attribute per file | `/sys/class/lab2_class/lab2/buffer_len` |
| `/proc/modules` | Loaded modules (what `lsmod` reads) | |
| `/proc/mtd` | MTD flash devices | LAB-02 Part C |

---

**Next:** [02 – Environment setup](02-environment-setup.md)
