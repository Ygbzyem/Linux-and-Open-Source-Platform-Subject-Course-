# Exercise 01 – Store data in upper case ★★

**English** | [Tiếng Việt](README.vi.md) · [← Exercises](../README.md)

## Task

Every string written to `/dev/lab2` must be **stored** in upper case. Digits and symbols stay
unchanged.

## Where to change

| Region | Change |
|---|---|
| ⓪ includes | `#include <linux/ctype.h>` – kernel version of `toupper` (no libc in the kernel) |
| Ⓐ `lab2_write` | Loop over `device_buffer` **after** `copy_from_user` succeeds, **before** `mutex_unlock` |

```c
size_t i;
/* … after copy_from_user succeeded, still holding lab2_mutex … */
for (i = 0; i < len; i++)
    device_buffer[i] = toupper(device_buffer[i]);
```

## Test (QEMU)

```sh
echo "Hello Lab2 123!" > /dev/lab2
cat /dev/lab2                                  # HELLO LAB2 123!
grep 'Last data' /proc/lab2_info               # [HELLO LAB2 123!
cat /sys/class/lab2_class/lab2/last_data       # HELLO LAB2 123!
```

Log: [`test_log.txt`](test_log.txt)

## Pitfalls

| Mistake | Why it is wrong |
|---|---|
| Transforming `buf[i]` | `buf` is a `__user` pointer – never dereference it in the kernel |
| Transforming in `lab2_read` | `cat` shows upper case, but `/proc` and sysfs still show the stored lower case |
| `i <= len` | Touches one byte past the data; only "works" because the next line writes `'\0'` there |
| Loop after `mutex_unlock` | Modifies shared data without the lock |

## Viva questions

1. Why `<linux/ctype.h>` and not `<ctype.h>`? → The kernel does not link against libc.
2. Why must the loop run after `copy_from_user`? → Before that the data is still in user memory.
3. What does `echo "xin chào"` give? → `XIN CHàO`: `toupper` works per byte on ASCII only; UTF-8
   bytes above 127 are left unchanged.
4. Store vs display: change `write` to store, change `read` to only display.
