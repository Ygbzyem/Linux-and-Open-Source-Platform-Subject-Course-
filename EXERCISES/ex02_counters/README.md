# Exercise 02 – Count reads and writes ★★

**English** | [Tiếng Việt](README.vi.md) · [← Exercises](../README.md)

## Task

Add two counters and show them in `/proc/lab2_info`:

- `write_count` – +1 on every **successful** write
- `read_count` – +1 on every `read` that **returns data** (the EOF call returning 0 is not counted)

## Where to change

| Region | Change |
|---|---|
| ⓪ globals | `static int write_count = 0;` and `static int read_count = 0;` next to `open_count` |
| Ⓐ `lab2_write` | `write_count++` after `copy_from_user` succeeds, before `mutex_unlock` |
| Ⓐ `lab2_read` | Take the mutex, single exit via `goto out`, `read_count++` on the data path |
| Ⓑ `lab2_proc_show` | Two `seq_printf` lines |

`lab2_read` with the single-exit pattern:

```c
static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    if (*offset >= buffer_len) { ret = 0; goto out; }          /* EOF – not counted */
    if (len > buffer_len - *offset)
        len = buffer_len - *offset;
    if (copy_to_user(buf, device_buffer + *offset, len)) { ret = -EFAULT; goto out; }
    *offset += len;
    read_count++;
    ret = len;                       /* bytes sent in THIS call, not the position */
out:
    mutex_unlock(&lab2_mutex);
    return ret;
}
```

## Test (QEMU)

```sh
echo hi > /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2
grep count /proc/lab2_info          # Open 3, Write 1, Read 2
echo -n abc > /dev/lab2 ; dd if=/dev/lab2 bs=1 2>/dev/null
grep count /proc/lab2_info          # Open 5, Write 2, Read 5
```

`dd bs=1` reads one byte per call: 3 calls return data (+3), one returns 0 (not counted).

Log: [`test_log.txt`](test_log.txt)

## Pitfalls

| Mistake | Effect |
|---|---|
| `mutex_unlock` without `mutex_lock` | Unlocking a lock you don't hold corrupts it – two processes may enter the critical section |
| `ret = *offset` instead of `ret = len` | Same value on the first call only; with `dd bs=1` the reader gets garbage bytes |
| Counter declared inside the function | Re-created and reset to 0 on every call |
| `read_count++` at the top of `read` | Also counts the EOF call → each `cat` counts 2 |
| Forgetting `seq_printf` | Counters work but are invisible |

## Viva questions

1. Why must `count++` be under the mutex? → `++` is read–add–write; two CPUs can interleave and lose an increment.
2. How many times does `cat` call `read` on a 3-byte buffer? → Twice: 3 bytes, then 0 (EOF).
3. What does `read` return? → Bytes copied in this call; 0 = EOF; negative = error.
