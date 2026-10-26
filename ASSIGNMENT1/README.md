# ASSIGNMENT-01 – How to Write a Device Driver

**English** | [Tiếng Việt](README.vi.md) · [← Home](../README.md)

| Task | Content | Points | Status |
|---|---|---|---|
| 1 | Extend the LAB-02 driver with **ioctl** (4 commands) | 40 | ✅ 24/24 tests pass |
| 2 | New **misc device** `counter_driver` | 30 | ✅ 25/25 tests pass |
| 3 | Design analysis report | 20 | – |
| 4 | Four theory questions | 10 | ✅ See [docs/07](../docs/07-viva-qa.md) C1, E1, F, G1 |

---

## 📁 Contents

```text
ASSIGNMENT1/
├── task1_ioctl/
│   ├── asgn1_ioctl.h      ← shared kernel ↔ user "contract": magic, struct, 4 commands
│   ├── asgn1_driver.c     ← driver: char device + procfs + sysfs + unlocked_ioctl
│   ├── asgn1_app.c        ← user-space tool issuing ioctl calls
│   ├── test_asgn1.sh      ← automated test, prints PASS/FAIL
│   └── Makefile           ← builds both the module (.ko) and the app (ARM, static)
├── task2_misc/
│   ├── counter_driver.c   ← misc device /dev/counter + /proc/counter_info
│   ├── test_counter.sh    ← automated test, prints PASS/FAIL
│   └── Makefile
└── logs/
    ├── test_asgn1_output.txt
    └── test_counter_output.txt
```

---

# Task 1 – ioctl interface

## 🏗️ Architecture

```text
 USER SPACE                                    KERNEL
 ┌────────────────────┐                         ┌──────────────────────────────────────┐
 │ echo / cat         │── read()/write() ──────►│ asgn1_read / asgn1_write             │
 │                    │                         │   KERNEL mode → prepend "[KERNEL] "  │
 │ asgn1_app          │── ioctl(fd, cmd, &x) ──►│ asgn1_ioctl                          │
 │   reset | stats    │                         │   switch(cmd): RESET / GET_STATS /   │
 │   mode N | version │◄── copy_to_user ────────│   SET_MODE / GET_VERSION / -ENOTTY   │
 └─────────┬──────────┘                         │                                      │
           │ #include                           │ shared state: device_buffer, counters│
           ▼                                    │ mode – all guarded by asgn1_mutex    │
   ┌───────────────┐   #include                 │                                      │
   │ asgn1_ioctl.h │◄───────────────────────────│ /proc/asgn1_info, /sys/class/...     │
   └───────────────┘                            └──────────────────────────────────────┘
```

| Property | Value |
|---|---|
| Module | `asgn1_driver.ko` |
| Device | `/dev/asgn1`, major **245** |
| procfs | `/proc/asgn1_info` |
| sysfs | `/sys/class/asgn1_class/asgn1/` |

## 📜 The four ioctl commands

| Command | Macro | Hex | Data | Behaviour |
|---|---|---|---|---|
| `ASGN1_RESET_BUFFER` | `_IO(0xA5, 0)` | `0x0000a500` | – | Clear buffer, `write_count = read_count = 0` |
| `ASGN1_GET_STATS` | `_IOR(0xA5, 1, struct asgn1_stats)` | `0x8014a501` | 20 bytes → user | Five statistics fields |
| `ASGN1_SET_MODE` | `_IOW(0xA5, 2, int)` | `0x4004a502` | 4 bytes → kernel | 0 = KERNEL (prefix), 1 = ECHO (default); anything else → `-EINVAL` |
| `ASGN1_GET_VERSION` | `_IOR(0xA5, 3, char[64])` | `0x8040a503` | 64 bytes → user | Version string containing the student ID |
| Any other | | | | `-ENOTTY` |

Decoding `0x8014a501`:

```text
 0x8014a501 = 10 | 00000000010100 | 10100101 | 00000001
              DIR      SIZE           TYPE        NR
              2=READ   0x14 = 20      0xA5        1
                       = sizeof(struct asgn1_stats) (5 × int)
```

## ⚡ Build and run

```bash
cd ASSIGNMENT1/task1_ioctl
make                     # → asgn1_driver.ko + asgn1_app (ARM, statically linked)
../../tools/dev.sh       # build + repack + boot QEMU
```

Inside QEMU:

```sh
test_asgn1.sh
```

Try each command by hand:

```sh
insmod /lib/modules/5.15.221/asgn1_driver.ko
asgn1_app version
echo hello > /dev/asgn1 ; cat /dev/asgn1          # hello
asgn1_app mode 0
echo hello > /dev/asgn1 ; cat /dev/asgn1          # [KERNEL] hello
asgn1_app stats                                   # buffer_len = 15, last_write_size = 6
asgn1_app mode 5                                  # ioctl SET_MODE: Invalid argument
asgn1_app invalid                                 # Inappropriate ioctl for device
asgn1_app reset ; cat /dev/asgn1                  # (empty)
dmesg | grep ioctl
```

Record a log while testing:

```bash
script -q -c "$HOME/embedded_lab2/dev.sh" ../logs/test_asgn1_output.txt
```

## ✅ Test results

```text
[1] Load module ............................ 4 PASS
[2] Default mode (ECHO) .................... 1 PASS
[3] ASGN1_GET_VERSION ...................... 1 PASS
[4] SET_MODE 0 (KERNEL prefix) ............. 2 PASS
[5] ASGN1_GET_STATS ........................ 2 PASS
[6] SET_MODE 1 ............................. 2 PASS
[7] Error handling (-EINVAL, -ENOTTY) ...... 3 PASS
[8] Oversized write (2000 bytes) ........... 2 PASS
[9] RESET_BUFFER ........................... 4 PASS
[10] Unload module ......................... 3 PASS
 RESULT: 24 passed, 0 failed
```

Full log: [`logs/test_asgn1_output.txt`](logs/test_asgn1_output.txt)

## 🧠 Design decisions

| Decision | Reason |
|---|---|
| Shared `asgn1_ioctl.h`, `#ifdef __KERNEL__` selects the include | Single source for command codes and struct → the two sides cannot drift apart |
| Check `_IOC_TYPE(cmd) != ASGN1_MAGIC` before the `switch` | Reject commands meant for other drivers immediately |
| `GET_STATS`: copy into a local struct under the mutex → **unlock** → `copy_to_user` | `copy_to_user` may page-fault and sleep; never sleep holding the lock |
| `memset(&st, 0, …)` before returning the struct | Avoid leaking stale kernel stack bytes to user space |
| `GET_VERSION`: `strscpy` into a 64-byte buffer, copy exactly 64 bytes | Never overrun the user buffer; the string is always NUL-terminated |
| `read`/`write` use the `goto out` pattern | A single `mutex_unlock` → impossible to leak the lock |
| `write` reads `mode` **under** the mutex | `mode` can be changed by ioctl at any time |
| `memcpy` for the prefix, `copy_from_user` for user data | `memcpy` is correct when both ends are kernel memory |
| `last_write_size` = user bytes, `buffer_len` includes the prefix | The prefix is added by the driver, not written by the user |
| `write` always returns `len`, even when truncating | Returning less makes the shell call `write` again and **overwrite** the data |
| `asgn1_app` exits with status 1 on error | Lets the test script tell "correctly rejected" from "unexpectedly succeeded" |

## 🐞 Bugs found and how they were debugged

**Long write lost its beginning.** Writing 2000 characters left only 977 bytes in
`/proc/asgn1_info`.

```text
asgn1_driver: write too large, truncating
asgn1_driver: received 1023 bytes (write #4)
asgn1_driver: received 977 bytes (write #5)      ← the shell retried on its own
Data length     : 977 bytes
```

Cause: the old `write` clamped `len` to 1023 and **returned 1023** (< 2000). User space took it
as a partial write → called again with the remaining 977 bytes → the driver overwrote the
buffer from the start. Fix: compute a separate `copy_len`, always return `len`. After the fix:

```text
asgn1_driver: write 2000 bytes truncated to 1023
asgn1_driver: received 2000 bytes, mode=1, stored 1023 bytes (write #4)
Data length     : 1023 bytes
```

**NUL terminator in the wrong place.** `device_buffer[buffer_len] = '\0'` ran **before**
`buffer_len` was updated → it used the previous write's length → old data leaked through.
Fix: terminate at the new length.

---

# Task 2 – Misc device `counter_driver`

## 🏗️ Design

```text
 cat /dev/counter ──► counter_read  ── *ppos == 0 ? ──► val = atomic_fetch_inc(&counter)
                                                       snapshot "val\n" in private_data
                                     └─ serve the snapshot from *ppos, 0 at its end (EOF)
 echo N > /dev/counter ──► counter_write ──► kstrtoint ──► N < 0 or not a number → -EINVAL
                                                         else atomic_set(&counter, N)
 cat /proc/counter_info ──► current value, total reads, total writes
```

| Property | Value |
|---|---|
| Module | `counter_driver.ko` |
| Registration | `misc_register()` / `misc_deregister()` |
| Device | `/dev/counter`, major **10**, dynamic minor, mode 0666 |
| State | `atomic_t counter, total_reads, total_writes` – no mutex needed |
| procfs | `/proc/counter_info` |

| Decision | Reason |
|---|---|
| Misc device instead of `register_chrdev` | Major 10 is shared by all misc devices; the kernel assigns the minor and creates `/dev/counter`. No class/device boilerplate |
| `atomic_fetch_inc` (get **and** increment in one step) | Separate `atomic_read` + `atomic_inc` would let two concurrent readers get the same value |
| Per-open snapshot in `file->private_data` (allocated in `open`, freed in `release`) | `cat` calls `read` twice: the second call must return EOF without incrementing again; `dd bs=1` must see one consistent string |
| Bounded 16-byte kernel buffer for `write` | User input never overflows kernel memory; longer input → `-EINVAL` |
| Rejected writes are not counted in `total_writes` | Only successful updates change the statistics |
| `init` deregisters the misc device if `proc_create` fails | Undo completed steps on error; `exit` runs in reverse order |

## ⚡ Build and run

```bash
cd ASSIGNMENT1/task2_misc
../../tools/dev.sh
```

```sh
test_counter.sh
```

## ✅ Test results

```text
[1] Load module (misc major 10, /proc/misc) ... 5 PASS
[2] Read auto-increment 0 → 1 → 2 .............. 3 PASS
[3] Statistics before write ..................... 2 PASS
[4] Write 100 → reads 100, 101 .................. 3 PASS
[5] Reset (write 0) ............................. 2 PASS
[6] Invalid input 'abc', '-5' → -EINVAL ......... 3 PASS
[7] dd bs=1: whole value, single increment ...... 2 PASS
[8] Final statistics (9 reads, 2 writes) ........ 2 PASS
[9] Unload module ............................... 3 PASS
 RESULT: 25 passed, 0 failed
```

`/proc/counter_info` at the three required moments (from the log):

```text
before write   Current value : 3    Total reads : 3   Total writes : 0
after write    Current value : 102  Total reads : 5   Total writes : 1
after reset    Current value : 1    Total reads : 6   Total writes : 2
```

Full log: [`logs/test_counter_output.txt`](logs/test_counter_output.txt)
