# Exercise 02b – Rename the driver to required names ★★

**English** | [Tiếng Việt](README.vi.md) · [← Exercises](../README.md)

## Task

Same functionality as `base`, with these names:

| Item | Required |
|---|---|
| Module | `se204386_drv` |
| Device | `/dev/tungdev` |
| Class | `tung_class` |
| proc file | `/proc/tung_info` |
| Major | `242` |

## Where to change

| Item | Change |
|---|---|
| Module | Rename `lab2_driver.c` → `se204386_drv.c` **and** `obj-m += se204386_drv.o` |
| Device | `#define DEVICE_NAME "tungdev"` |
| Class | `#define CLASS_NAME "tung_class"` |
| proc | `proc_create("tung_info", …)` |
| Major | `#define DRIVER_MAJOR 242` |

Check: `grep -n '"lab2\|240\|lab2_info' se204386_drv.c` must print nothing.

## Test (QEMU)

```sh
insmod /lib/modules/5.15.221/se204386_drv.ko     # not loaded by rcS – load it yourself
lsmod                                             # lab2_driver + se204386_drv
grep -E '^240|^242' /proc/devices                 # 240 lab2, 242 tungdev
ls -l /dev/tungdev                                # 242, 0
cat /proc/tung_info
echo hello > /dev/tungdev ; cat /dev/tungdev
rmmod se204386_drv
```

Log: [`test_log.txt`](test_log.txt)

## Pitfalls

| Mistake | Effect |
|---|---|
| Renamed the file but not `obj-m` (or the reverse) | `No rule to make target 'X.o'` |
| Kept major 240 | `insmod: Device or resource busy` – `lab2_driver` loaded by `rcS` holds 240 |
| Expecting `rcS` to load it | `rcS` only loads `lab2_driver.ko` |
| Kept `test_driver.sh` | It tests `/dev/lab2`, not the renamed device |

## Viva questions

1. Where does the module name come from? → The source file name (`X.c` → `X.ko`).
2. What is a major number? → Index into the kernel's table of registered char drivers; VFS reads it
   from the device file's inode and calls that driver's `file_operations`. The minor selects a device
   inside the driver.
3. Why can't two drivers share major 240? → One table slot holds one `file_operations`;
   the second `register_chrdev(240, …)` returns `-EBUSY`.
