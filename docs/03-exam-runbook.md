# 03 – Exam runbook ⭐

**English** | [Tiếng Việt](vi/03-quy-trinh-thi.md) · [← Home](../README.md)

Typical exam task: *"Modify the driver so it does X, build it, run it on QEMU and explain it."*
This page is a **step-by-step checklist** with copy-paste commands.

---

## 0. The 5-step workflow – know it by heart

```text
 LAPTOP (x86)                                              QEMU (ARM)
 ─────────────────────────────────────────────────────     ──────────────────────
 ① edit .c →  ② make  →  ③ copy into rootfs  →  ④ repack  →  ⑤ boot → insmod → test
                └──────────────── dev.sh does ②③④⑤ ───────────────┘
```

| Step | If you forget it |
|---|---|
| ② `make` | No new `.ko` |
| ③ copy into rootfs | QEMU runs the old `.ko` |
| ④ repack | QEMU runs the **whole old rootfs** → `not found` |
| ⑤ check the prompt | You type test commands on the laptop instead of QEMU |

> 🚫 **Never run `qemu-system-arm` directly after changing code.** Always use `dev.sh`.

**Which prompt am I at?**

| Prompt | Where |
|---|---|
| `tung_pham@tung-pham:~/...$` | Laptop |
| `/ # ` | Inside QEMU |

---

## 1. Check the environment (1 minute, first thing in the exam)

```bash
ls ~/embedded_lab1/kernel/linux-5.15.221/Makefile
ls ~/embedded_lab1/output/zImage ~/embedded_lab1/output/vexpress-v2p-ca9.dtb
ls -l ~/embedded_lab2/dev.sh
arm-linux-gnueabihf-gcc --version | head -1
sudo -v
```

Any `No such file` → see [02 – Environment setup](02-environment-setup.md).

> 💡 `sudo -v` asks for the password up front. If it fails: switch off the Vietnamese input
> method, check Caps Lock, and type rather than paste.

---

## 2. Create a working directory from the base driver

Base driver = `LAB2/embedded_lab2/driver/lab2_driver.c` (Parts A + B, known to work).

```bash
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
mkdir -p ~/exam && cp -r $REPO/LAB2/embedded_lab2/driver ~/exam/task1
cd ~/exam/task1
make clean >/dev/null 2>&1; ls
```

> 💡 **Keep the name `lab2_driver`** unless the task says otherwise: `rcS` will
> **automatically `insmod`** the new build at boot, and `test_driver.sh` keeps working.
> If you rename it or change the major → you must `insmod`/`mknod` yourself.

If the task requires renaming the driver:

```bash
mv lab2_driver.c mydrv.c
sed -i 's/lab2_driver\.o/mydrv.o/' Makefile
sed -i 's/lab2_/mydrv_/g; s/"lab2"/"mydrv"/' mydrv.c
grep -n 'lab2\|240' mydrv.c          # fix the remaining spots by hand (major…)
```

---

## 3. Edit the code – what the task asks → where to change

Driver file map (details: [04 – Driver anatomy](04-driver-anatomy.md)):

```text
 ⓪ #include, #define, globals            ← add new variables here
 Ⓐ open / read / write / release + fops ← data processing, counters, sync
 Ⓑ procfs + sysfs                        ← exposing information
 ⓘ init / exit                           ← register extra attributes / proc / device
```

| Task asks for | Change | Key API |
|---|---|---|
| Transform data on write (upper-case, reverse, filter…) | Ⓐ `write`, after `copy_from_user` | `toupper`, `isalnum` (`<linux/ctype.h>`) |
| Transform data on read | Ⓐ `read`, into a temp buffer before `copy_to_user` | |
| Count reads/writes/opens | ⓪ new variable + Ⓐ increment **under the mutex** | |
| Show info in `/proc` | Ⓑ `*_proc_show`, add `seq_printf` | `seq_printf` |
| Read-only sysfs attribute | Ⓑ `xxx_show` + `DEVICE_ATTR_RO(xxx)` + add to attrs array | `sprintf` |
| Writable sysfs attribute | Ⓑ `xxx_show` + `xxx_store` + `DEVICE_ATTR_RW(xxx)` | `kstrtoint` |
| Sysfs "button" (write-only) | Ⓑ `xxx_store` + `DEVICE_ATTR_WO(xxx)` | |
| Parameter at `insmod` time | ⓪ `module_param` | `module_param(name, int, 0444)` |
| Kernel-assigned major | ⓘ `register_chrdev(0, …)`, keep the return value | |
| Only one process may open | Ⓐ `open` returns `-EBUSY`, `release` frees | `atomic_t`, `mutex_trylock` |
| `read` blocks until data arrives | Ⓐ wait queue | `wait_event_interruptible` |
| Control commands (ioctl) | Ⓐ `unlocked_ioctl` + shared header + user-space app | `_IO/_IOR/_IOW` |

Function signatures: [05 – Kernel API](05-kernel-api.md).

**Mandatory pattern when adding logic to a function holding a mutex** – single exit point:

```c
ssize_t ret;
mutex_lock(&lab2_mutex);
if (error) { ret = -EFAULT; goto out; }
/* … */
ret = len;
out:
mutex_unlock(&lab2_mutex);
return ret;
```

---

## 4. Build and run

```bash
cd ~/exam/task1
make 2>&1 | grep -E 'error|warning'     # must print nothing
~/embedded_lab2/dev.sh
```

Reading `make` errors: find the first `file.c:LINE:COL: error:` line → open the file at that
line. Common errors: [06 – Troubleshooting](06-troubleshooting.md).

Before QEMU boots, `dev.sh` lists the files inside the package – **your `.ko` must be there**.

---

## 5. Test inside QEMU – commands by task type

**Is the driver loaded?**

```sh
lsmod
grep lab2 /proc/devices
ls -l /dev/lab2
dmesg | tail -20
```

**Read / write:**

```sh
echo hello > /dev/lab2
cat /dev/lab2
cat /dev/lab2            # second time – checks EOF/offset handling
```

**procfs / sysfs:**

```sh
cat /proc/lab2_info
ls -l /sys/class/lab2_class/lab2/
cat /sys/class/lab2_class/lab2/buffer_len
echo 1 > /sys/class/lab2_class/lab2/<rw_attribute>
```

**Manual load / unload:**

```sh
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=5     # if there is a module_param
cat /sys/module/lab2_driver/parameters/max_len
```

**Large / special input:**

```sh
head -c 2000 /dev/zero | tr '\0' x > /dev/lab2
printf 'abc' > /dev/lab2                  # no trailing \n
```

**Hold the device open (single-open / blocking tests):**

```sh
sleep 30 < /dev/lab2 &       # background process keeps the file open for 30 s
cat /dev/lab2                # try a second open
cat /dev/lab2 &              # blocking read in the background
echo hi > /dev/lab2          # wake it up
```

Quit QEMU: **Ctrl+A, then X**.

---

## 6. Save a log to submit / show

Record the whole session (including everything inside QEMU):

```bash
cd ~/exam/task1
script -q -c "$HOME/embedded_lab2/dev.sh" ~/exam/task1/log_task1.txt
```

Run the tests in QEMU, then Ctrl+A X → the log is saved.

---

## 7. Checklist before calling the examiner

- [ ] `make` prints no `warning`
- [ ] Every `return` after `mutex_lock` goes through `mutex_unlock`
- [ ] User ↔ kernel data only goes through `copy_to_user` / `copy_from_user`
- [ ] `read` updates `*offset` → `cat` terminates
- [ ] Everything created in `init` is destroyed in `exit`, **in reverse order**
- [ ] `rmmod` works and `lsmod` no longer lists the module
- [ ] You can explain the path: `echo` → syscall → VFS → `fops->write` → `copy_from_user`

---

## 8. Suggested time budget (60-minute exam)

| Minutes | Task |
|---|---|
| 0–5 | Check environment, create working directory |
| 5–30 | Edit code, get a clean build |
| 30–45 | Test in QEMU, fix bugs |
| 45–55 | Save log, go through the checklist |
| 55–60 | Prepare your explanation (see [07 – Viva Q&A](07-viva-qa.md)) |

---

**Next:** [04 – Driver anatomy](04-driver-anatomy.md)
