# Exercise 05 – Allow only one opener ★★★

**English** | [Tiếng Việt](README.vi.md) · [← Exercises](../README.md)

## Task

Only one process may have `/dev/lab2` open at a time. A second `open` returns **`-EBUSY`**;
after the first process closes the file it can be opened again. `open_count` counts only
successful opens. `/proc/lab2_info` shows `In use : yes/no`.

## Where to change

| Region | Change |
|---|---|
| ⓪ | `#include <linux/atomic.h>`, `static atomic_t in_use = ATOMIC_INIT(0);` |
| Ⓐ `lab2_open` | `atomic_cmpxchg(&in_use, 0, 1)`; old value ≠ 0 → `return -EBUSY`; count only after acquiring |
| Ⓐ `lab2_release` | `atomic_set(&in_use, 0);` |
| Ⓑ `lab2_proc_show` | `In use : %s` with `atomic_read(&in_use) ? "yes" : "no"` |

```c
static int lab2_open(struct inode *inode, struct file *file)
{
    if (atomic_cmpxchg(&in_use, 0, 1) != 0) {   /* test-and-set in ONE step */
        pr_info("lab2_driver: busy\n");
        return -EBUSY;
    }
    mutex_lock(&lab2_mutex);
    open_count++;                              /* only successful opens */
    mutex_unlock(&lab2_mutex);
    return 0;
}

static int lab2_release(struct inode *inode, struct file *file)
{
    atomic_set(&in_use, 0);
    return 0;
}
```

## Test (QEMU)

```sh
sleep 30 < /dev/lab2 &               # background process keeps the device open
grep -E 'Open|In use' /proc/lab2_info   # Open count 1, In use yes
cat /dev/lab2                        # can't open '/dev/lab2': Device or resource busy
echo hi > /dev/lab2                  # can't create /dev/lab2: Device or resource busy
kill $!                              # killed process → release still runs
grep -E 'Open|In use' /proc/lab2_info   # In use no, Open count still 1
echo hi > /dev/lab2 ; cat /dev/lab2  # hi
```

Log: [`test_log.txt`](test_log.txt)

## Pitfalls

| Mistake | Effect |
|---|---|
| `int busy; if (busy) return -EBUSY; busy = 1;` | Check and set are two steps → two CPUs can both get in |
| Missing `}` of the `if` in `open` | Dozens of `invalid storage class for function` errors for every later function |
| `atomic_cmpxchg` with 2 arguments in `release` | Build error – release only needs `atomic_set(&in_use, 0)` |
| Releasing at the end of `read`/`write` | Another process can open while the file is still open |
| `open_count++` before the check | Rejected opens are counted too |

## Viva questions

1. Why `atomic_cmpxchg`? → Compare and set in one indivisible operation; returns the old value.
2. Is the device locked forever if the holder is killed? → No – the kernel closes all its files, so `release` runs.
3. Why does `test_driver.sh` still pass? → Its opens are sequential, never overlapping.
