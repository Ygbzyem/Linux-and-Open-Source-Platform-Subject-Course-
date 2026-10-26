# Exercise 04 – Load-time parameter `max_len` ★★

**English** | [Tiếng Việt](README.vi.md) · [← Exercises](../README.md)

## Task

Add a module parameter `max_len` given at `insmod` time:

- Maximum bytes stored per write. Default **1023**, valid **1–1023**.
- Out of range → `insmod` fails with `-EINVAL`.
- Longer input is truncated **without** the retry/overwrite bug.
- Show `max_len` in `/proc/lab2_info`.

## Where to change

| Region | Change |
|---|---|
| ⓪ globals (after the `#define`s) | `static int max_len = BUFFER_SIZE - 1;` `module_param(max_len, int, 0444);` `MODULE_PARM_DESC(...)` |
| ⓘ start of `lab2_init` | Validate **before** `register_chrdev` – nothing to undo on failure |
| Ⓐ `lab2_write` | Copy `copy_len = min_t(size_t, len, max_len)` bytes, **return `len`** |
| Ⓑ `lab2_proc_show` | `Max length : %d` |

```c
static int __init lab2_init(void)
{
    if (max_len < 1 || max_len > BUFFER_SIZE - 1) {
        pr_err("lab2_driver: invalid max_len=%d (must be 1..%d)\n", max_len, BUFFER_SIZE - 1);
        return -EINVAL;
    }
    /* register_chrdev … */
}
```

## sysfs attribute vs module parameter

| | Ex 03: sysfs `mode` | Ex 04: `module_param` |
|---|---|---|
| Set when | Any time, **while running** | Once, at `insmod` |
| How | `echo 1 > /sys/class/…/mode` | `insmod lab2_driver.ko max_len=5` |
| Read back | `/sys/class/lab2_class/lab2/mode` | `/sys/module/lab2_driver/parameters/max_len` |
| Change | `echo` again | `rmmod` + `insmod` |

## Test (QEMU)

```sh
cat /sys/module/lab2_driver/parameters/max_len        # 1023 (rcS loads without params)
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=5
echo abcdefgh > /dev/lab2 ; cat /dev/lab2              # abcde
dmesg | tail -4                                         # ONE "truncated", ONE "received"
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=1  # accepted (lower bound)
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=0     # Invalid argument
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=1024  # Invalid argument
```

Log: [`test_log.txt`](test_log.txt)

## Pitfalls (all happened while doing this exercise)

| Mistake | Effect |
|---|---|
| `MODULE_PARM_DES` | Build error – the macro is `MODULE_PARM_DESC` |
| `pr_err();` with no format | Build error; and a log without the value hides the cause |
| `pr_info("[%s]", copy_len)` | `%s` treats a number as a pointer → possible kernel oops |
| `max_len <= 1 && max_len >= 1023` | `&&` is never true → no validation at all |
| `max_len <= 1 \|\| max_len >= 1023` | Rejects the valid bounds 1 and 1023 → default fails, driver not loaded at boot |
| Default 1024 | `device_buffer[1024] = '\0'` writes past the 1024-byte array |
| Returning `copy_len` from `write` | Shell retries the rest → buffer holds only the tail |

Always test the five boundary points: 0, 1, middle, 1023, 1024.

## Viva questions

1. When is `max_len` assigned? → By the kernel from the `insmod` command line, **before** `lab2_init` runs.
2. Why validate before `register_chrdev`? → Nothing has been created yet, so a plain `return -EINVAL` needs no cleanup.
3. What does `0444` mean here? → `/sys/module/lab2_driver/parameters/max_len` is read-only at run time.
4. Why can `rcS` fail to load the driver after this change? → `rcS` loads without parameters; if the default is invalid, `init` rejects it.
