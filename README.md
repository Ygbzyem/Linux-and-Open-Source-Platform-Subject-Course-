# 🐧 Embedded Linux – A Learning Handbook

**English** | [Tiếng Việt](README.vi.md)

> My notes and work for the **Embedded Linux Systems** course (FPT University), starting from
> zero: building a kernel, booting an ARM system on QEMU, writing kernel drivers, and the
> course assignments.
>
> Everything here is written to be **understood** and **reproducible step by step**, even
> without an internet connection.

**Author:** Pham Minh Tung – SE204386

---

## 📌 How to use this repository

| I want to… | Read |
|---|---|
| Understand what Embedded Linux is and how the system boots | [`docs/01-what-is-embedded-linux.md`](docs/01-what-is-embedded-linux.md) |
| Rebuild the whole environment on a fresh machine | [`docs/02-environment-setup.md`](docs/02-environment-setup.md) |
| **Exam: modify a driver → build → run → test as fast as possible** | [`docs/03-exam-runbook.md`](docs/03-exam-runbook.md) ⭐ |
| See what a character driver is made of | [`docs/04-driver-anatomy.md`](docs/04-driver-anatomy.md) |
| Look up a kernel API (signature, header, example) | [`docs/05-kernel-api.md`](docs/05-kernel-api.md) |
| Fix an error – search by symptom | [`docs/06-troubleshooting.md`](docs/06-troubleshooting.md) |
| Prepare for "why…?" oral questions | [`docs/07-viva-qa.md`](docs/07-viva-qa.md) |

---

## 🧰 Environment

| Component | Version / configuration |
|---|---|
| Host | Ubuntu 26.04 x86_64, 12 cores |
| Cross-compiler | `arm-linux-gnueabihf-gcc` 15.2.0 |
| Emulated board | QEMU `vexpress-a9` (ARM Cortex-A9, 512 MB RAM, 2 CPUs) |
| Target kernel | **Linux 5.15.221** (`vexpress_defconfig`) |
| Bootloader | U-Boot 2022.04 |
| Root filesystem | BusyBox 1.35.0 (static), packed as an initramfs |

> ⚠️ There is no physical board – everything runs on QEMU. The **host** kernel (Ubuntu) and
> the **target** kernel (ARM inside QEMU) are two completely different kernels.

---

## 🗺️ Learning path

```text
  LAB-01  Build a minimal Embedded Linux system
          toolchain → kernel → U-Boot → BusyBox → boot on QEMU
             │
             ▼
  LAB-02  Write a character device driver
          file_operations → procfs/sysfs → MTD/JFFS2 → auto-load at boot
             │
             ▼
  ASSIGNMENT-01  Extend the driver
          Task 1: ioctl (4 commands) │ Task 2: misc device
             │
             ▼
  Driver exercises  (modify the LAB-02 driver on demand – exam style)
             │
             ▼
  ASSIGNMENT-02  Sensor Monitoring System (capstone)
          kernel driver + multi-threaded app + IPC + debugging + real-time
```

---

## 📊 Progress

| Part | Content | Status | Docs |
|---|---|---|---|
| LAB-01 | Kernel, U-Boot, BusyBox, QEMU boot | ✅ Direct QEMU boot | [LAB1](LAB1/README.md) |
| LAB-02 | Char driver, procfs/sysfs, MTD/JFFS2, auto-load | ✅ Parts A–D | [LAB2](LAB2/README.md) |
| ASSIGNMENT-01 Task 1 | ioctl interface | ✅ 24/24 tests pass | [ASSIGNMENT1](ASSIGNMENT1/README.md) |
| ASSIGNMENT-01 Task 2 | misc device `counter_driver` | ⏳ In progress | |
| Driver exercises | Exam-style driver modifications | 📝 Planned | |
| ASSIGNMENT-02 | Sensor Monitoring System | 📝 Planned | |

---

## 📁 Repository layout

```text
.
├── README.md                ← you are here (README.vi.md: Vietnamese)
├── docs/                    ← concepts + procedures (readable offline); docs/vi/: Vietnamese
├── tools/
│   └── dev.sh               ← build → copy into rootfs → repack → boot QEMU
├── LAB1/
│   ├── README.md
│   └── embedded_lab1/
│       ├── configs/         ← kernel, U-Boot and BusyBox .config files
│       ├── rootfs_custom/   ← hand-written rootfs files (rcS, inittab, …)
│       ├── boot_log.txt
│       └── uboot_log.txt
├── LAB2/
│   ├── README.md
│   └── embedded_lab2/
│       ├── driver/          ← lab2_driver.c, Makefile, test scripts
│       ├── rootfs_custom/
│       └── logs/
└── ASSIGNMENT1/
    ├── README.md
    ├── task1_ioctl/         ← asgn1_driver.c, asgn1_ioctl.h, asgn1_app.c, test
    ├── task2_misc/
    └── logs/
```

**Intentionally NOT in the repo** (too large or reproducible): kernel, U-Boot and BusyBox
sources, the built rootfs, `zImage`, `*.ko`, `*.cpio.gz`. To recreate them, see
[`docs/02-environment-setup.md`](docs/02-environment-setup.md).

---

## ⚡ Quick start (environment already set up)

```bash
cd ASSIGNMENT1/task1_ioctl        # or any driver directory with a Makefile
../../tools/dev.sh                # build + repack + boot QEMU
```

Inside QEMU (prompt `/ # `):

```sh
insmod /lib/modules/5.15.221/asgn1_driver.ko
test_asgn1.sh
```

Quit QEMU: **Ctrl+A, then X**.

Fixed paths on my machine (scripts and Makefiles rely on them):

| Path | What it is |
|---|---|
| `~/embedded_lab1/kernel/linux-5.15.221` | Built kernel source (`KDIR` in every Makefile) |
| `~/embedded_lab1/output/` | `zImage`, `vexpress-v2p-ca9.dtb` |
| `~/embedded_lab2/rootfs/initramfs/` | Working rootfs |
| `~/embedded_lab2/output/initramfs_lab2.cpio.gz` | Packed rootfs |

---

## 🔎 Offline lookup

```bash
# Search all documentation
grep -rn --include='*.md' 'copy_to_user' .

# Check the real signature of a kernel function in the local source tree
grep -rn 'int misc_register' ~/embedded_lab1/kernel/linux-5.15.221/include/

# Browse real in-tree drivers
ls ~/embedded_lab1/kernel/linux-5.15.221/drivers/misc/
```

---

## 📝 Commit convention

This repo follows [Conventional Commits](https://www.conventionalcommits.org/):
types `feat`, `fix`, `refactor`, `test`, `docs`, `build`, `chore`, `style` with scopes
`lab1`, `lab2`, `asgn1`, `asgn2`, `tools`, `docs`, `repo`.

```text
feat(asgn1): add ioctl interface with 4 commands
fix(tools): handle filenames starting with dash in dev.sh
```

---

## 📄 License

[MIT](LICENSE). Assignment handouts (`*.docx`) belong to the course instructor.
