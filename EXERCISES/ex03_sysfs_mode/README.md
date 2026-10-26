# Exercise 03 – Writable sysfs attribute `mode` ★★★

**English** | [Tiếng Việt](README.vi.md) · [← Exercises](../README.md)

## Task

Add a read/write attribute `/sys/class/lab2_class/lab2/mode`:

| `mode` | Data written to `/dev/lab2` is stored |
|---|---|
| `0` (default) | Unchanged |
| `1` | UPPER CASE |
| `2` | lower case |
| anything else | Rejected with `-EINVAL` |

## Where to change

| Region | Change |
|---|---|
| ⓪ | `#include <linux/ctype.h>`, `static int mode = 0;` |
| Ⓐ `lab2_write` | After `copy_from_user`: `toupper` if mode 1, `tolower` if mode 2 (still under the mutex) |
| Ⓑ sysfs | `mode_show`, `mode_store`, `static DEVICE_ATTR_RW(mode);` – **after** both functions |
| Ⓑ `lab2_attrs[]` | `&dev_attr_mode.attr,` before `NULL` |

```c
static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    int v;

    if (kstrtoint(buf, 10, &v) || v < 0 || v > 2)
        return -EINVAL;              /* validate first … */
    mutex_lock(&lab2_mutex);
    mode = v;                        /* … then change state */
    mutex_unlock(&lab2_mutex);
    return count;                    /* bytes consumed – NOT 0 */
}
static DEVICE_ATTR_RW(mode);         /* 0644, needs mode_show + mode_store */
```

## Test (QEMU)

```sh
cat /sys/class/lab2_class/lab2/mode                      # 0
echo "Hello World" > /dev/lab2 ; cat /dev/lab2           # Hello World
echo 1 > /sys/class/lab2_class/lab2/mode
echo "Hello World" > /dev/lab2 ; cat /dev/lab2           # HELLO WORLD
echo 2 > /sys/class/lab2_class/lab2/mode
echo "Hello World" > /dev/lab2 ; cat /dev/lab2           # hello world
echo 5 > /sys/class/lab2_class/lab2/mode                 # write error: Invalid argument
echo abc > /sys/class/lab2_class/lab2/mode               # write error: Invalid argument
cat /sys/class/lab2_class/lab2/mode                      # 2 (unchanged)
ls -l /sys/class/lab2_class/lab2/mode                    # -rw-r--r--
```

Log: [`test_log.txt`](test_log.txt)

## Pitfalls (both happened while doing this exercise)

| Mistake | Effect |
|---|---|
| No `return` at the end of `mode_store` | `error: control reaches end of non-void function` (kernel builds with `-Werror=return-type`) |
| `return 0;` in `mode_store` | `echo 1 > mode` **hangs**: 0 bytes consumed → echo retries forever (mode still changes) |
| Copy-pasted the mode-1 block for mode 2 and kept `toupper` | Builds fine, mode 2 gives upper case – only caught by testing every branch |
| `DEVICE_ATTR_RW` placed before the functions | `undeclared` errors |
| Not adding `&dev_attr_mode.attr` to the array | Builds, but no `mode` file in sysfs |
| `copy_from_user` on the `_store` buffer | Wrong – sysfs already copied it into kernel memory (`const char *buf`, no `__user`) |

## Viva questions

1. Why `return count`? → A store/write returns the bytes it consumed; less makes the caller retry.
2. Why is `buf` read directly here but not in `lab2_write`? → sysfs hands `_store` a kernel buffer;
   `write` receives a `__user` pointer.
3. Why validate before assigning? → Invalid input must leave the state untouched.
4. sysfs vs ioctl? → sysfs is text and works from the shell; ioctl needs a C program but can pass structs.
