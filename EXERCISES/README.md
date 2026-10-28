# Driver exercises (LAB-02 based)

**English** | [Tiếng Việt](README.vi.md) · [← Home](../README.md)

Exam-style tasks: modify the LAB-02 character driver to do something new, build it, run it on
QEMU and explain it. Every exercise starts from a fresh copy of [`base/`](base/) and keeps the
module name `lab2_driver`, so `rcS` loads it automatically at boot.

> 📘 Full exercise handbook (20 + 8 exercises, headers ↔ functions, patterns): [`HANDBOOK.md`](HANDBOOK.md)

## Workflow for each exercise

```bash
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
cp -r "$REPO/EXERCISES/base" "$REPO/EXERCISES/exNN_name"
cd "$REPO/EXERCISES/exNN_name"
code lab2_driver.c                                   # make the change
make 2>&1 | grep -E 'error|warning'                  # must print nothing
script -q -c "$HOME/embedded_lab2/dev.sh" test_log.txt
```

How to approach a new task: [`docs/03-exam-runbook.md`](../docs/03-exam-runbook.md).

## Exercises

| # | Exercise | Difficulty | Layer changed | Key API | Status |
|---|---|---|---|---|---|
| 01 | [Store data in upper case](ex01_toupper/README.md) | ★★ | `write` | `toupper` | ✅ |
| 02 | [Count reads and writes](ex02_counters/README.md) | ★★ | `read`, `write`, procfs | mutex, `goto out`, `seq_printf` | ✅ |
| 02b | [Rename the driver to required names](ex02b_rename/README.md) | ★★ | names, major | `obj-m`, `DEVICE_NAME`, `CLASS_NAME` | ✅ |
| 03 | [Writable sysfs attribute `mode`](ex03_sysfs_mode/README.md) | ★★★ | sysfs, `write` | `DEVICE_ATTR_RW`, `kstrtoint` | ✅ |
| 04 | [Load-time parameter `max_len`](ex04_module_param/README.md) | ★★ | `module_param`, `init`, `write` | `module_param`, `min_t` | ✅ |
| 05 | [Allow only one opener](ex05_single_open/README.md) | ★★★ | `open`, `release` | `atomic_cmpxchg`, `-EBUSY` | ✅ |

> After practising, restore the base driver in the rootfs:
> `cd "$REPO/EXERCISES/base" && ~/embedded_lab2/dev.sh`
