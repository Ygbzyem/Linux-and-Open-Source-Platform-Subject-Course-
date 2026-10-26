# 07 – Viva Q&A

**English** | [Tiếng Việt](vi/07-van-dap.md) · [← Home](../README.md)

Each question has a **short answer** (20–30 seconds out loud) and a **deeper explanation**
(for follow-up questions).

---

## A. The Embedded Linux system

### A1. What are the four components of an Embedded Linux system?

**Short:** Toolchain, bootloader, kernel, root filesystem.

**Deeper:** The toolchain (`arm-linux-gnueabihf-gcc`) lives on the host and builds the other
three. The bootloader (U-Boot) initialises RAM and loads the kernel. The kernel manages
hardware, processes and memory. The rootfs (BusyBox) contains `/sbin/init` and the user commands.

### A2. Describe the path from power-on to the `/ #` prompt.

**Short:** QEMU loads kernel + dtb + initramfs → kernel starts and unpacks the initramfs →
runs `/sbin/init` → reads `/etc/inittab` → runs `rcS` → opens a shell.

**Deeper:** `rcS` (action `::sysinit:`) mounts `proc`, `sysfs`, `devtmpfs` and `insmod`s the
driver; then `::askfirst:-/bin/sh` prints "Please press Enter" and starts the shell. On a real
board, ROM code and U-Boot run before the kernel.

### A3. What is the Device Tree for?

**Short:** It describes the hardware (where RAM is, which address the UART is at…) to the
kernel, because ARM hardware cannot describe itself the way PCI does on a PC.

**Deeper:** The kernel matches the `compatible` strings in the `.dtb` against drivers. One
`zImage` can therefore run on many boards – only the `.dtb` changes.

### A4. Why must your own C programs be built with `-static`?

**Short:** The static BusyBox rootfs has no shared libraries (`libc.so`). A dynamic binary fails
with `not found` because the dynamic loader does not exist.

---

## B. Kernel modules & drivers

### B1. What is a major number?

**Short:** The driver's identifier. When `/dev/lab2` (major 240) is opened, VFS uses 240 to
find the `file_operations` registered by `register_chrdev(240, …)`.

**Deeper:** The minor number distinguishes devices handled by the same driver.
`ls -l /dev/lab2` → `240, 0`.

### B2. What is `file_operations`?

**Short:** The table of function pointers the driver hands to the kernel: a `read` system call
on the device file is routed by VFS to `fops->read`.

### B3. Order in `init` and `exit`? Why is `exit` reversed?

**Short:** init: `register_chrdev` → `class_create` → `device_create`. exit does the reverse
because later objects depend on earlier ones – a device lives inside its class, so the device
must be destroyed before the class.

### B4. What if `class_create` fails in `init`?

**Short:** Undo the steps that succeeded (`unregister_chrdev`) and return the error code.
Otherwise the major stays taken forever and the next `insmod` fails with
`Device or resource busy`.

### B5. What is vermagic? Why does `insmod` say `Invalid module format`?

**Short:** A "fingerprint" of the kernel build (`5.15.221 SMP mod_unload ARMv7 p2v8`). A module
only loads into a kernel with a matching vermagic → build the module against **the same
source tree** that built the kernel (`KDIR` in the Makefile).

### B6. How is `devtmpfs` related to `/dev/lab2`?

**Short:** When `device_create()` runs, the kernel creates the `/dev/lab2` node in devtmpfs –
no `mknod` needed. That is why `rcS` uses `[ -c /dev/lab2 ] || mknod …` to avoid `File exists`.

---

## C. User ↔ kernel memory

### C1. Why `copy_to_user` instead of `memcpy` or dereferencing the pointer? (Task 4 – Q2)

**Short:** A user-space pointer is a **virtual address of the calling process**; it may be
invalid, may point into kernel memory, or may point to a page that is not in RAM.
`copy_*_user` validates the address and handles faults safely; `memcpy` does not.

**Deeper (MMU level):**

- Every process has its own page tables; the MMU translates virtual → physical addresses. A
  user address only means something in that process's context.
- `copy_*_user` checks the address lies in user space (`access_ok`). Without that check a
  malicious program could pass a kernel address and read/write kernel memory.
- The user page may not be resident yet (lazy allocation, swap). Touching it causes a
  **page fault**. `copy_*_user` uses the exception table: a fault on a valid address is
  resolved and the copy continues; on an invalid address the function returns the number of
  bytes not copied → the driver returns `-EFAULT`. The same fault inside `memcpy` causes a
  **kernel oops**.
- On ARM with PAN (Privileged Access Never) the kernel is forbidden from touching user memory
  directly; `copy_*_user` temporarily lifts that restriction in a controlled way.

### C2. What does `copy_to_user` return?

**Short:** The number of bytes **not** copied. 0 = success. Non-zero → return `-EFAULT`.

### C3. When is `memcpy` correct inside a driver?

**Short:** When **both** ends are kernel memory – e.g. copying the `"[KERNEL] "` prefix (a
constant inside the module) into `device_buffer`.

---

## D. Synchronisation

### D1. What is the mutex for?

**Short:** Two processes writing at the same time (or one writing, one reading) both touch
`buffer` and `buffer_len` → race condition. The mutex guarantees only one process is in the
critical section at a time. Similar to `xSemaphoreTake` in FreeRTOS.

### D2. Why release the mutex **before** `copy_to_user` in ioctl GET_STATS?

**Short:** `copy_to_user` can page-fault and put the process to **sleep**. Holding the mutex
while sleeping blocks everyone else for nothing. Copy into a local struct under the lock →
unlock → `copy_to_user`.

### D3. Why `memset(&st, 0, sizeof(st))` before returning the struct?

**Short:** The local struct lives on the kernel stack and may contain stale bytes (including
padding). Not clearing it **leaks kernel data** to user space.

### D4. Mutex or atomic?

**Short:** `atomic_t` for **one** counter/flag. A mutex when **several** variables must change
together (buffer + len) or the operation may sleep.

### D5. Why the `goto out` pattern?

**Short:** With several error paths, each doing its own `mutex_unlock`, it is easy to forget one
→ the next call hangs forever. `goto out` gives a single exit; `mutex_unlock` is written once.

---

## E. read / write / offset

### E1. What is `*ppos` in `read`? How does VFS call `read` repeatedly? (Task 4 – Q4)

**Short:** `*ppos` is the current read position **for that open file**; VFS stores it in
`struct file` and passes it on every call. The driver reads from `*ppos` and advances it by the
number of bytes returned. `cat` calls `read()` until it receives **0** (EOF), then stops.

**Deeper:**

```text
cat /dev/asgn1   (6-byte buffer)
  read(fd, buf, 4096)  ppos=0 → driver returns 6, ppos=6
  read(fd, buf, 4096)  ppos=6 → *ppos >= buffer_len → returns 0 → cat stops
```

The **driver** decides when to stop by returning 0. Not updating `*ppos` → data from the start
every time → infinite loop. Each new `open` starts again at `ppos = 0`.

### E2. What happens if `write` returns less than `len`?

**Short:** User space treats it as a partial write and calls `write` again with the rest. With
a driver that overwrites from the start of the buffer, the tail overwrites the head. Observed in
practice: `received 1023` then `received 977`, the buffer kept only 977 bytes.

---

## F. Module reference counting (Task 4 – Q1)

### F1. When does a module's reference count go up and down?

**Short:** It goes up when something is **using** the module's code – typically a process that
has `/dev/asgn1` open; it goes down when that stops (file closed).

**Deeper:** Thanks to `.owner = THIS_MODULE` in `file_operations`, VFS calls `try_module_get()`
on a successful `open` and `module_put()` on `release`. Another module using this module's
symbols also raises the count. See the third column of `lsmod`.

### F2. What happens on `rmmod` while a process holds the device open?

**Short:** Refused because the refcount is > 0 (BusyBox prints something like
`rmmod: can't unload module 'asgn1_driver': …` – record the real message when you try it).
Without this, the kernel would remove the code while a process could still call `read` → jump
into freed memory → oops.

Try it:

```sh
sleep 60 < /dev/asgn1 &
lsmod                 # "used by" column = 1
rmmod asgn1_driver    # refused
kill %1; rmmod asgn1_driver   # works
```

### F3. How do you force-unload a module?

**Short:** The right way: find and stop the process holding the file (`kill`), then `rmmod`.
Forced: `rmmod -f`, which only works if the kernel has `CONFIG_MODULE_FORCE_UNLOAD` – **very
dangerous**, since the code disappears while it is still in use → likely kernel crash. Debug only.

---

## G. MTD and JFFS2 (Task 4 – Q3)

### G1. How is MTD different from a block device?

**Short:** A block device (SSD, SD card, USB stick) has its **own controller** that handles
erasing, wear levelling and bad blocks → the OS just sees sectors it can read/write freely.
MTD is **raw** flash without such a controller → the kernel must do that work.

| | Block device | MTD (raw flash) |
|---|---|---|
| Operations | read / write sectors | read / write pages / **erase blocks** |
| Overwrite in place | Yes | No – erase the whole block first (bits only go 1 → 0) |
| Wear levelling, bad blocks | Device controller | Filesystem/kernel |
| Examples | `/dev/sda`, `/dev/mmcblk0` | `/dev/mtd1` |
| Filesystems | ext4, FAT | JFFS2, UBIFS |

**When MTD is mandatory:** when NAND/NOR flash chips are soldered directly onto the board with
no controller (routers, cameras, low-cost IoT devices). You then need a flash-aware filesystem
such as JFFS2/UBIFS – ext4 on raw flash wears out frequently written blocks quickly.

### G2. Why mount JFFS2 via `/dev/mtdblock1` if MTD is not a block device?

**Short:** `mount` expects a block device; `mtdblock` is a thin emulation layer on top of MTD
that satisfies `mount`. Underneath, JFFS2 still talks to MTD directly.

### G3. What is `85 19` at the start of the flash hexdump?

**Short:** The JFFS2 magic `0x1985` (stored little-endian). Every node starts with it;
`01 e0` = DIRENT node (file name), `02 e0` = INODE node (data). JFFS2 is log-structured: there
is no fixed table, so mounting has to scan the whole flash.

---

## H. ioctl

### H1. Why ioctl when read/write already exist?

**Short:** read/write carry **data**; ioctl carries **control** (change mode, reset, fetch
statistics) – commands that are not a stream of bytes.

### H2. What is the magic number for?

**Short:** An 8-bit driver identifier embedded in every command. A command meant for another
driver will not match → `-ENOTTY` instead of executing something by accident.

### H3. What is the third argument of `_IOR(magic, nr, type)` for?

**Short:** The macro encodes `sizeof(type)` into the 14-bit SIZE field. E.g.
`ASGN1_GET_STATS = 0x8014a501` → DIR=2 (read), SIZE=0x14=20 bytes = `sizeof(struct asgn1_stats)`,
TYPE=0xA5, NR=1.

### H4. Why return `-ENOTTY` for unknown commands?

**Short:** POSIX convention for "ioctl not appropriate for this device"; user space `perror`
prints `Inappropriate ioctl for device`.

---

**Next:** back to the [README](../README.md) or see [ASSIGNMENT-01](../ASSIGNMENT1/README.md).
