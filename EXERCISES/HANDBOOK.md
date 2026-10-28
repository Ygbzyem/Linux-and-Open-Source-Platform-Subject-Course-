# LAB-02 driver exercise handbook

**English** | [Tiếng Việt](HANDBOOK.vi.md) · [← Exercises](README.md) · [← Home](../README.md)

20 exercises + 8 variants built on the LAB-02 driver. Each one lists: **task, regions to change, headers, functions, core code, test commands, pitfalls**.

> Kernel 5.15.221 · QEMU vexpress-a9 · `arm-linux-gnueabihf-gcc` 15.2
> Exercises already solved in this repo link to their folder. The others are reference code written against the base driver – run them before the exam.

## Contents

1. [Workflow for one exercise](#1-workflow-for-one-exercise)
2. [Driver map and "task → where to change"](#2-driver-map-and-task--where-to-change)
3. [Which header – which function](#3-which-header--which-function)
4. [Four reusable patterns](#4-four-reusable-patterns)
5. [Twenty exercises](#5-twenty-exercises)
6. [Variants](#6-variants)
7. [Required names](#7-required-names)
8. [Error codes](#8-error-codes)
9. [Checklist before calling the examiner](#9-checklist-before-calling-the-examiner)

## 1. Workflow for one exercise

| Step | Action | Command |
|---|---|---|
| ① | Copy the base driver | `cp -r $REPO/EXERCISES/base ~/exam/bai1` |
| ② | Edit the code | `code ~/exam/bai1/lab2_driver.c` |
| ③ | Clean build | `make 2>&1 \| grep -E 'error\|warning'` (must print nothing) |
| ④ | Copy + repack + boot | `script -q -c "$HOME/embedded_lab2/dev.sh" log.txt` |
| ⑤ | Test in QEMU (prompt `/ # `) | Each exercise's test commands · quit: Ctrl+A then X |

- Keep the name `lab2_driver` unless the task says otherwise: `rcS` loads the new build and `test_driver.sh` still works.
- Never run `qemu-system-arm` directly after editing code – always use `dev.sh`.

```bash
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
mkdir -p ~/exam && cp -r "$REPO/EXERCISES/base" ~/exam/bai1
cd ~/exam/bai1
make 2>&1 | grep -E 'error|warning'
script -q -c "$HOME/embedded_lab2/dev.sh" log_bai1.txt
```

### Reading make errors

| Message | Meaning |
|---|---|
| First `file.c:LINE:COL: error:` | The place to fix – later lines are usually consequences |
| Many `invalid storage class for function` | Missing `}` in the function before the first error |
| `'x' undeclared` / `implicit declaration` | Undeclared variable or missing `#include` (see section 3) |
| `control reaches end of non-void function` | Missing `return` at the end |
| `format '%s' expects … char *` | Wrong type in `pr_info`/`seq_printf` – can crash the kernel |

## 2. Driver map and "task → where to change"

```text
 ⓪ DATA            #include, MODULE_*, #define, static variables, DEFINE_MUTEX
 Ⓐ OPERATIONS      lab2_open, lab2_release, lab2_read, lab2_write, lab2_fops
 Ⓑ EXPORT          lab2_proc_show + proc_ops │ *_show/*_store + DEVICE_ATTR_* + lab2_attrs[]
 ⓘ REGISTRATION    lab2_init (register_chrdev → class_create → device_create → sysfs → proc)
                   lab2_exit (in REVERSE order)
```

| The task asks to… | Change | Examples |
|---|---|---|
| Transform data on write (case, filter, encode) | Ⓐ `write`, after `copy_from_user` | 1, 5, B1, B2 |
| Transform only on read | Ⓐ `read`, scratch buffer before `copy_to_user` | 2 |
| Count / statistics | ⓪ variable + Ⓐ update under mutex + Ⓑ print | 6, 7, 8, B4 |
| Record caller / time | Ⓐ `write` with `current`, `ktime_*` | 9, 10, B7 |
| Configure at run time | Ⓑ sysfs `_RW` / `_WO` | 11, 12, B3 |
| Configure at insmod | ⓪ `module_param` + ⓘ validate at start of init | 13, B5 |
| Change registration | ⓘ init/exit | 14, B6 |
| Access limits / synchronisation | Ⓐ open/release, locks, wait queue | 15, 16, 18 |
| New data structure (queue) | ⓪ + Ⓐ read/write | 17 |
| Control commands | Ⓐ `unlocked_ioctl` + header + app | 19 |
| Several devices | ⓘ + Ⓐ using `private_data` | 20 |

## 3. Which header – which function

Got `implicit declaration of function 'X'`? Find X in the table and add that `#include`. The kernel does **not** use the normal C `<stdio.h>`, `<ctype.h>`, `<string.h>`.

| Header | Main functions / macros | Use for |
|---|---|---|
| `<linux/module.h>` | MODULE_LICENSE, MODULE_AUTHOR, module_init, module_exit, THIS_MODULE | Every module – always |
| `<linux/fs.h>` | register_chrdev, unregister_chrdev, struct file_operations, iminor, default_llseek | Registering a char device, fops table |
| `<linux/device.h>` | class_create, device_create, device_destroy, DEVICE_ATTR_RO/RW/WO, sysfs_create_group, ATTRIBUTE_GROUPS | Automatic /dev node, sysfs attributes |
| `<linux/uaccess.h>` | copy_to_user, copy_from_user, get_user, put_user | All user ↔ kernel data |
| `<linux/mutex.h>` | DEFINE_MUTEX, mutex_init, mutex_lock, mutex_unlock, mutex_trylock, mutex_lock_interruptible | Protecting several variables together |
| `<linux/atomic.h>` | atomic_t, ATOMIC_INIT, atomic_read/set/inc/dec, atomic_cmpxchg, atomic_fetch_inc | A single counter / flag |
| `<linux/proc_fs.h> + <linux/seq_file.h>` | proc_create, proc_remove, struct proc_ops, single_open, single_release, seq_read, seq_lseek, seq_printf | /proc/xxx file |
| `<linux/moduleparam.h>` | module_param, MODULE_PARM_DESC | Parameters given at insmod |
| `<linux/kernel.h>` | kstrtoint, kstrtouint, min, max, min_t, clamp, sprintf, snprintf, scnprintf, pr_info/pr_warn/pr_err | String → number, comparisons, logging |
| `<linux/string.h>` | strlen, strscpy, memset, memcpy, strcmp | Strings in the kernel |
| `<linux/ctype.h>` | toupper, tolower, isalpha, isdigit, isalnum, isspace, isupper, islower | Transforming / classifying characters |
| `<linux/sched.h>` | current->pid, current->comm, TASK_COMM_LEN | Which process is calling |
| `<linux/timekeeping.h>, <linux/ktime.h>` | ktime_get_real_seconds, ktime_get_ns, ktime_get, ktime_to_ms | Time |
| `<linux/jiffies.h>` | jiffies, jiffies_to_msecs, HZ | Tick count, uptime |
| `<linux/wait.h>` | DECLARE_WAIT_QUEUE_HEAD, wait_event_interruptible, wake_up_interruptible | Blocking read |
| `<linux/slab.h>` | kmalloc, kzalloc, kfree, GFP_KERNEL | Dynamic memory |
| `<linux/miscdevice.h>` | struct miscdevice, MISC_DYNAMIC_MINOR, misc_register, misc_deregister | Misc device (major 10) |
| `<linux/ioctl.h> (kernel) / <sys/ioctl.h> (user)` | _IO, _IOR, _IOW, _IOC_TYPE, _IOC_NR, _IOC_SIZE | ioctl commands |
| `<linux/random.h>` | get_random_u32 | Random numbers |

Look up the real signature on your machine:

```bash
K=~/embedded_lab1/kernel/linux-5.15.221
grep -rn 'int misc_register' $K/include/
grep -rn 'define DEVICE_ATTR_RW' $K/include/
```

## 4. Four reusable patterns

### 4.1. Single exit for a function holding a mutex

```c
ssize_t ret;
mutex_lock(&lab2_mutex);
if (error_1) { ret = 0;       goto out; }
if (error_2) { ret = -EFAULT; goto out; }
/* normal work */
ret = len;
out:
mutex_unlock(&lab2_mutex);
return ret;
```

Check: no `return` between `mutex_lock` and `out:`; every branch sets `ret`.

### 4.2. write truncates but still returns len

```c
size_t copy_len = min_t(size_t, len, BUFFER_SIZE - 1);   /* bytes STORED */
/* copy_from_user(device_buffer, buf, copy_len) … */
buffer_len = copy_len;
device_buffer[buffer_len] = '\0';                       /* at the NEW length */
return len;                                             /* bytes the user SENT */
```

Returning less than `len` → the shell calls `write` again with the rest and overwrites the data.

### 4.3. sysfs attribute

```c
static ssize_t xxx_show(struct device *d, struct device_attribute *a, char *buf)
{ return sprintf(buf, "%d\n", xxx); }

static ssize_t xxx_store(struct device *d, struct device_attribute *a,
                         const char *buf, size_t count)
{
    int v;
    if (kstrtoint(buf, 10, &v) || /* out of range */) return -EINVAL;
    mutex_lock(&lab2_mutex); xxx = v; mutex_unlock(&lab2_mutex);
    return count;
}
static DEVICE_ATTR_RW(xxx);      /* RO: _show only (0444) · WO: _store only (0200) */
/* lab2_attrs[]: &dev_attr_xxx.attr,   – before NULL */
```

### 4.4. Adding a line to /proc

```c
seq_printf(m, "Name          : %d\n", an_int);
seq_printf(m, "Name          : %lu\n", an_unsigned_long);
seq_printf(m, "Name          : %s\n", flag ? "yes" : "no");
```

| Type | Format |
|---|---|
| `int` | `%d` |
| `unsigned int` | `%u` |
| `long` / `unsigned long` | `%ld` / `%lu` |
| `size_t` / `ssize_t` | `%zu` / `%zd` |
| `long long` (`time64_t`, `s64`) | `%lld` |
| `u64` | `%llu` |
| string / char | `%s` / `%c` |

## 5. Twenty exercises

| # | Exercise | Difficulty | Solved |
|---|---|---|---|
| 1 | Store data in UPPER CASE | ★★☆ | ✅ `ex01_toupper` |
| 2 | Read back REVERSED | ★★☆ | – |
| 3 | APPEND mode | ★★☆ | – |
| 4 | Read once, then clear (consume-on-read) | ★★☆ | – |
| 5 | Character filter | ★★☆ | – |
| 6 | Count reads / writes | ★★☆ | ✅ `ex02_counters` |
| 7 | Total bytes written (sysfs RO) | ★★☆ | – |
| 8 | Content statistics in /proc | ★★☆ | – |
| 9 | Last writer (PID + process name) | ★★☆ | – |
| 10 | Time of last write | ★★☆ | – |
| 11 | WRITABLE sysfs attribute: mode | ★★★ | ✅ `ex03_sysfs_mode` |
| 12 | CLEAR button via sysfs (write-only) | ★★☆ | – |
| 13 | Load-time parameter: max_len | ★★☆ | ✅ `ex04_module_param` |
| 14 | Kernel-assigned major | ★★☆ | – |
| 15 | Allow only ONE opener | ★★★ | ✅ `ex05_single_open` |
| 16 | Lock INTERRUPTIBLE by Ctrl+C | ★★☆ | – |
| 17 | FIFO message queue (ring buffer) | ★★★ | – |
| 18 | BLOCKING read (wait queue) | ★★★ | – |
| 19 | ioctl commands: CLEAR and GET_LEN | ★★★ | ✅ `ASSIGNMENT1/task1_ioctl` |
| 20 | Two devices /dev/lab2_0 and /dev/lab2_1 | ★★★ | – |

### Exercise 1. Store data in UPPER CASE ★★☆

**Task:** Every string written to /dev/lab2 is STORED in upper case; digits and symbols unchanged.

**Header:** `<linux/ctype.h>` · **Main functions:** `toupper(c)`

| Region | What to do |
|---|---|
| ⓪ | #include <linux/ctype.h> |
| Ⓐ write | After a successful copy_from_user, before mutex_unlock: toupper loop |

**Core code**

```c
size_t i;
/* in lab2_write, after a successful copy_from_user, still holding the mutex */
for (i = 0; i < len; i++)
    device_buffer[i] = toupper(device_buffer[i]);
```

**Test in QEMU**

```sh
echo "Hello Lab2 123!" > /dev/lab2 ; cat /dev/lab2     # HELLO LAB2 123!
grep 'Last data' /proc/lab2_info                      # upper case
```

**Pitfalls**

- i <= len → touches one byte past the data
- Modifying buf (a __user pointer) → wrong
- Converting in read → /proc still shows lower case

> Full solution: [`ex01_toupper`](ex01_toupper/)

### Exercise 2. Read back REVERSED ★★☆

**Task:** The buffer keeps the original, but read returns it reversed; a trailing '\n' stays at the end.

**Header:** `<linux/uaccess.h>`, `<linux/mutex.h>` · **Main functions:** `copy_to_user`, `mutex_lock/unlock`

| Region | What to do |
|---|---|
| ⓪ | Scratch buffer static char rev_buf[BUFFER_SIZE] (do not put 1 KB on the kernel stack) |
| Ⓐ read | Take the mutex, build the reversed copy in rev_buf, copy_to_user from rev_buf + *offset |

**Core code**

```c
static char rev_buf[BUFFER_SIZE];

static ssize_t lab2_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret;
    int i, n, m;

    mutex_lock(&lab2_mutex);
    if (*off >= buffer_len) { ret = 0; goto out; }
    n = buffer_len;
    m = (n > 0 && device_buffer[n - 1] == '\n') ? n - 1 : n;   /* skip trailing '\n' */
    for (i = 0; i < m; i++)
        rev_buf[i] = device_buffer[m - 1 - i];
    if (m < n)
        rev_buf[m] = '\n';            /* put '\n' back at the end */
    if (len > n - *off) len = n - *off;
    if (copy_to_user(buf, rev_buf + *off, len)) { ret = -EFAULT; goto out; }
    *off += len;
    ret = len;
out:
    mutex_unlock(&lab2_mutex);
    return ret;
}
```

**Test in QEMU**

```sh
echo abc > /dev/lab2 ; cat /dev/lab2         # cba
cat /proc/lab2_info | grep 'Last data'      # [abc  (buffer unchanged)
```

**Pitfalls**

- Reversing the '\n' too → prompt ends up at the start of the line
- Building the copy without the mutex → mixes in data being written

### Exercise 3. APPEND mode ★★☆

**Task:** write appends to the end of the buffer instead of overwriting; full → -ENOSPC.

**Header:** `<linux/uaccess.h>` · **Main functions:** `copy_from_user`

| Region | What to do |
|---|---|
| Ⓐ write | copy_from_user into device_buffer + buffer_len; check free space first |

**Core code**

```c
mutex_lock(&lab2_mutex);
if (buffer_len + len > BUFFER_SIZE - 1) { ret = -ENOSPC; goto out; }
if (copy_from_user(device_buffer + buffer_len, buf, len)) { ret = -EFAULT; goto out; }
buffer_len += len;
device_buffer[buffer_len] = '\0';
ret = len;
out:
mutex_unlock(&lab2_mutex);
return ret;
```

**Test in QEMU**

```sh
echo a > /dev/lab2 ; echo b > /dev/lab2 ; cat /dev/lab2      # a  then  b
head -c 2000 /dev/zero | tr '\0' x > /dev/lab2                  # No space left on device
```

**Pitfalls**

- Forgetting + buffer_len in the destination → still overwrites
- Checking space after the copy → already overflowed

### Exercise 4. Read once, then clear (consume-on-read) ★★☆

**Task:** After the data has been read completely the buffer is cleared: a second cat prints nothing.

**Header:** `<linux/uaccess.h>` · **Main functions:** `copy_to_user`

| Region | What to do |
|---|---|
| Ⓐ read | After the copy, if *offset reached the end, buffer_len = 0 |

**Core code**

```c
/* in lab2_read, after a successful copy_to_user and *off += len */
if (*off >= buffer_len) {
    buffer_len = 0;
    device_buffer[0] = '\0';
}
```

**Test in QEMU**

```sh
echo hi > /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2      # only the first prints "hi"
```

**Pitfalls**

- Clearing at the start of read → a reader with a small buffer (dd bs=1) gets only 1 byte
- Clearing without the mutex

### Exercise 5. Character filter ★★☆

**Task:** Store only letters and digits (keep the trailing '\n'); drop punctuation and spaces.

**Header:** `<linux/ctype.h>` · **Main functions:** `isalnum(c)`

| Region | What to do |
|---|---|
| ⓪ | #include <linux/ctype.h> |
| Ⓐ write | Filter in place with two indexes: i (read) and j (write) |

**Core code**

```c
size_t i, j = 0;
/* after a successful copy_from_user */
for (i = 0; i < len; i++)
    if (isalnum(device_buffer[i]) || device_buffer[i] == '\n')
        device_buffer[j++] = device_buffer[i];
buffer_len = j;
device_buffer[buffer_len] = '\0';
ret = len;               /* still return what the user sent */
```

**Test in QEMU**

```sh
echo "a-b c!1" > /dev/lab2 ; cat /dev/lab2        # abc1
```

**Pitfalls**

- Returning j instead of len → the shell retries
- In-place filtering works only because j <= i always

### Exercise 6. Count reads / writes ★★☆

**Task:** write_count grows on each successful write; read_count on each read that returns data (EOF not counted); show both in /proc.

**Header:** `<linux/mutex.h>`, `<linux/seq_file.h>` · **Main functions:** `mutex_lock`, `seq_printf`

| Region | What to do |
|---|---|
| ⓪ | static int write_count, read_count |
| Ⓐ write/read | Increment under the mutex, read uses the goto-out pattern |
| Ⓑ proc | Two seq_printf lines |

**Core code**

```c
/* lab2_read – data path */
*off += len;
read_count++;
ret = len;            /* NOT *off */

/* lab2_proc_show */
seq_printf(m, "Write count   : %d\n", write_count);
seq_printf(m, "Read count    : %d\n", read_count);
```

**Test in QEMU**

```sh
echo hi > /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2
grep count /proc/lab2_info              # Write 1, Read 2
```

**Pitfalls**

- mutex_unlock without mutex_lock
- ret = *offset (right only on the first call)
- Forgetting seq_printf → counters invisible

> Full solution: [`ex02_counters`](ex02_counters/)

### Exercise 7. Total bytes written (sysfs RO) ★★☆

**Task:** Attribute /sys/class/lab2_class/lab2/total_bytes accumulates every stored byte.

**Header:** `<linux/device.h>` · **Main functions:** `DEVICE_ATTR_RO`, `sprintf`

| Region | What to do |
|---|---|
| ⓪ | static unsigned long total_bytes |
| Ⓐ write | total_bytes += bytes stored |
| Ⓑ sysfs | total_bytes_show + DEVICE_ATTR_RO + add to lab2_attrs[] |

**Core code**

```c
static unsigned long total_bytes;

static ssize_t total_bytes_show(struct device *d, struct device_attribute *a, char *buf)
{
    return sprintf(buf, "%lu\n", total_bytes);
}
static DEVICE_ATTR_RO(total_bytes);
/* lab2_attrs[]:  &dev_attr_total_bytes.attr,   (before NULL) */
```

**Test in QEMU**

```sh
echo abc > /dev/lab2 ; echo de > /dev/lab2
cat /sys/class/lab2_class/lab2/total_bytes      # 7
```

**Pitfalls**

- %d for unsigned long → format warning (use %lu)
- Not adding it to the attrs array → no file

### Exercise 8. Content statistics in /proc ★★☆

**Task:** /proc/lab2_info shows the number of letters, digits and whitespace in the current data.

**Header:** `<linux/ctype.h>` · **Main functions:** `isalpha`, `isdigit`, `isspace`

| Region | What to do |
|---|---|
| Ⓑ proc | Only lab2_proc_show: walk device_buffer and count |

**Core code**

```c
int i, a = 0, d = 0, s = 0;
mutex_lock(&lab2_mutex);
for (i = 0; i < buffer_len; i++) {
    if (isalpha(device_buffer[i]))      a++;
    else if (isdigit(device_buffer[i])) d++;
    else if (isspace(device_buffer[i])) s++;
}
mutex_unlock(&lab2_mutex);
seq_printf(m, "Letters/Digits/Spaces : %d / %d / %d\n", a, d, s);
```

**Test in QEMU**

```sh
echo "ab 12 c" > /dev/lab2 ; grep Letters /proc/lab2_info    # 3 / 2 / 3  ('\n' is whitespace)
```

**Pitfalls**

- Forgetting '\n' is also isspace
- Counting without the lock → numbers off while someone writes

### Exercise 9. Last writer (PID + process name) ★★☆

**Task:** Remember the PID and name of the last writing process; expose it via sysfs last_writer.

**Header:** `<linux/sched.h>`, `<linux/string.h>` · **Main functions:** `current`, `strscpy`

| Region | What to do |
|---|---|
| ⓪ | #include <linux/sched.h>; static pid_t last_pid; static char last_comm[TASK_COMM_LEN] |
| Ⓐ write | Save current->pid, current->comm |
| Ⓑ sysfs | last_writer_show + DEVICE_ATTR_RO |

**Core code**

```c
/* lab2_write, under the mutex */
last_pid = current->pid;
strscpy(last_comm, current->comm, sizeof(last_comm));

static ssize_t last_writer_show(struct device *d, struct device_attribute *a, char *buf)
{
    return sprintf(buf, "%d %s\n", last_pid, last_comm);
}
static DEVICE_ATTR_RO(last_writer);
```

**Test in QEMU**

```sh
echo x > /dev/lab2 ; cat /sys/class/lab2_class/lab2/last_writer      # 123 sh
```

**Pitfalls**

- strcpy instead of strscpy → no length limit
- current is the calling process – meaningful only in open/read/write/ioctl

### Exercise 10. Time of last write ★★☆

**Task:** /proc/lab2_info shows how many seconds ago the last write happened.

**Header:** `<linux/timekeeping.h>` · **Main functions:** `ktime_get_real_seconds`

| Region | What to do |
|---|---|
| ⓪ | #include <linux/timekeeping.h>; static time64_t last_write_time |
| Ⓐ write | last_write_time = ktime_get_real_seconds() |
| Ⓑ proc | Print the difference |

**Core code**

```c
/* lab2_write, under the mutex */
last_write_time = ktime_get_real_seconds();

/* lab2_proc_show */
if (last_write_time)
    seq_printf(m, "Last write    : %lld s ago\n",
               (long long)(ktime_get_real_seconds() - last_write_time));
else
    seq_printf(m, "Last write    : never\n");
```

**Test in QEMU**

```sh
echo x > /dev/lab2 ; sleep 3 ; grep 'Last write' /proc/lab2_info     # 3 s ago
```

**Pitfalls**

- Printing time64_t with %d → wrong on 32-bit ARM (use %lld + cast to long long)
- Using float → no floating point in the kernel

### Exercise 11. WRITABLE sysfs attribute: mode ★★★

**Task:** mode 0 = as is, 1 = upper case, 2 = lower case; anything else → -EINVAL.

**Header:** `<linux/device.h>`, `<linux/kernel.h>`, `<linux/ctype.h>` · **Main functions:** `DEVICE_ATTR_RW`, `kstrtoint`

| Region | What to do |
|---|---|
| ⓪ | static int mode = 0; #include <linux/ctype.h> |
| Ⓐ write | Apply toupper/tolower by mode (under the mutex) |
| Ⓑ sysfs | mode_show + mode_store + DEVICE_ATTR_RW + add to the array |

**Core code**

```c
static ssize_t mode_store(struct device *d, struct device_attribute *a,
                          const char *buf, size_t count)
{
    int v;
    if (kstrtoint(buf, 10, &v) || v < 0 || v > 2)
        return -EINVAL;               /* validate first … */
    mutex_lock(&lab2_mutex);
    mode = v;                         /* … then change state */
    mutex_unlock(&lab2_mutex);
    return count;                     /* NOT 0 */
}
static DEVICE_ATTR_RW(mode);          /* needs mode_show too */
```

**Test in QEMU**

```sh
echo 1 > /sys/class/lab2_class/lab2/mode ; echo Hi > /dev/lab2 ; cat /dev/lab2   # HI
echo 5 > /sys/class/lab2_class/lab2/mode            # Invalid argument
```

**Pitfalls**

- return 0 in _store → echo hangs
- Missing return at the end → -Werror=return-type
- copy_from_user on the _store buf → wrong, it is already kernel memory

> Full solution: [`ex03_sysfs_mode`](ex03_sysfs_mode/)

### Exercise 12. CLEAR button via sysfs (write-only) ★★☆

**Task:** Writing 1 to /sys/class/lab2_class/lab2/clear empties the buffer.

**Header:** `<linux/device.h>`, `<linux/kernel.h>` · **Main functions:** `DEVICE_ATTR_WO`, `kstrtoint`

| Region | What to do |
|---|---|
| Ⓑ sysfs | clear_store + DEVICE_ATTR_WO + add to the array |

**Core code**

```c
static ssize_t clear_store(struct device *d, struct device_attribute *a,
                           const char *buf, size_t count)
{
    int v;
    if (kstrtoint(buf, 10, &v) || v != 1)
        return -EINVAL;
    mutex_lock(&lab2_mutex);
    buffer_len = 0;
    device_buffer[0] = '\0';
    mutex_unlock(&lab2_mutex);
    return count;
}
static DEVICE_ATTR_WO(clear);          /* mode 0200, needs only clear_store */
```

**Test in QEMU**

```sh
echo abc > /dev/lab2 ; echo 1 > /sys/class/lab2_class/lab2/clear ; cat /dev/lab2   # empty
cat /sys/class/lab2_class/lab2/clear       # Permission denied (no _show)
```

**Pitfalls**

- Also writing clear_show with DEVICE_ATTR_WO → unused warning
- Not checking v == 1 → any echo clears

### Exercise 13. Load-time parameter: max_len ★★☆

**Task:** insmod lab2_driver.ko max_len=N; default 1023, valid 1–1023, invalid → insmod fails.

**Header:** `<linux/moduleparam.h>`, `<linux/kernel.h>` · **Main functions:** `module_param`, `MODULE_PARM_DESC`, `min_t`

| Region | What to do |
|---|---|
| ⓪ (after #define) | static int max_len = BUFFER_SIZE - 1; module_param; MODULE_PARM_DESC |
| ⓘ start of init | Validate before register_chrdev |
| Ⓐ write | copy_len = min_t(size_t, len, max_len); return len |

**Core code**

```c
static int max_len = BUFFER_SIZE - 1;
module_param(max_len, int, 0444);
MODULE_PARM_DESC(max_len, "Max bytes stored per write");

/* start of lab2_init */
if (max_len < 1 || max_len > BUFFER_SIZE - 1) {
    pr_err("lab2_driver: invalid max_len=%d (must be 1..%d)\n", max_len, BUFFER_SIZE - 1);
    return -EINVAL;
}
```

**Test in QEMU**

```sh
rmmod lab2_driver ; insmod /lib/modules/5.15.221/lab2_driver.ko max_len=5
echo abcdefgh > /dev/lab2 ; cat /dev/lab2               # abcde
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=0   # Invalid argument
```

**Pitfalls**

- MODULE_PARM_DES (missing C)
- <= / >= at the bounds → rejects valid 1 and 1023
- Default 1024 → '\0' written past the array
- pr_info("%s", copy_len) → treats a number as a pointer

> Full solution: [`ex04_module_param`](ex04_module_param/)

### Exercise 14. Kernel-assigned major ★★☆

**Task:** Use register_chrdev(0, …) so the kernel picks a free major.

**Header:** `<linux/fs.h>` · **Main functions:** `register_chrdev(0`, `…)`

| Region | What to do |
|---|---|
| ⓘ init | Store the return value as major_number; use it in MKDEV and unregister_chrdev |

**Core code**

```c
ret = register_chrdev(0, DEVICE_NAME, &lab2_fops);
if (ret < 0)
    return ret;
major_number = ret;                 /* assigned major */
/* later code keeps using MKDEV(major_number, 0) */
```

**Test in QEMU**

```sh
grep lab2 /proc/devices          # e.g. 250 lab2
ls -l /dev/lab2                     # 250, 0 – devtmpfs created the right numbers
```

**Pitfalls**

- Still using DRIVER_MAJOR (240) somewhere → device_destroy on the wrong number
- Hard-coded mknod 240 in rcS → wrong node (devtmpfs already made the right one)

### Exercise 15. Allow only ONE opener ★★★

**Task:** A second open while the device is open → -EBUSY; after close it can be opened again.

**Header:** `<linux/atomic.h>` · **Main functions:** `atomic_cmpxchg`, `atomic_set`, `atomic_read`

| Region | What to do |
|---|---|
| ⓪ | #include <linux/atomic.h>; static atomic_t in_use = ATOMIC_INIT(0) |
| Ⓐ open | atomic_cmpxchg(&in_use, 0, 1), non-zero → -EBUSY |
| Ⓐ release | atomic_set(&in_use, 0) |

**Core code**

```c
static atomic_t in_use = ATOMIC_INIT(0);

/* lab2_open */
if (atomic_cmpxchg(&in_use, 0, 1) != 0)
    return -EBUSY;                      /* someone holds it */
/* … open_count++ only after acquiring */

/* lab2_release */
atomic_set(&in_use, 0);
```

**Test in QEMU**

```sh
sleep 30 < /dev/lab2 &
cat /dev/lab2              # Device or resource busy
kill $! ; cat /dev/lab2    # opens again
```

**Pitfalls**

- Plain int with if-then-assign → race on 2 CPUs
- Releasing at the end of read instead of release
- Missing } of the if → a cascade of invalid storage class errors

> Full solution: [`ex05_single_open`](ex05_single_open/)

### Exercise 16. Lock INTERRUPTIBLE by Ctrl+C ★★☆

**Task:** Replace mutex_lock with mutex_lock_interruptible in read/write.

**Header:** `<linux/mutex.h>` · **Main functions:** `mutex_lock_interruptible`

| Region | What to do |
|---|---|
| Ⓐ read/write | Take the lock in a signal-interruptible way; on failure return -ERESTARTSYS |

**Core code**

```c
if (mutex_lock_interruptible(&lab2_mutex))
    return -ERESTARTSYS;          /* interrupted while waiting – lock NOT taken */
/* … */
mutex_unlock(&lab2_mutex);
```

**Test in QEMU**

```sh
# normal tests from base: echo / cat must behave exactly the same
```

**Pitfalls**

- goto out (which unlocks) when the lock was never taken → unlocking a lock you don't hold
- Writing if (!mutex_lock_interruptible(...)) – inverted: 0 means SUCCESS

### Exercise 17. FIFO message queue (ring buffer) ★★★

**Task:** Keep up to 8 messages; each write pushes one at the tail, each cat pops one from the head; full → -ENOSPC.

**Header:** `<linux/uaccess.h>`, `<linux/kernel.h>` · **Main functions:** `copy_from_user`, `copy_to_user`, `min_t`

| Region | What to do |
|---|---|
| ⓪ | char msgs[8][128], size_t msg_len[8], head, tail, count |
| Ⓐ write | Store in msgs[tail], tail = (tail+1) % 8 |
| Ⓐ read | Take msgs[head], head = (head+1) % 8, one message per open |

**Core code**

```c
#define MSG_MAX 8
#define MSG_LEN 128
static char   msgs[MSG_MAX][MSG_LEN];
static size_t msg_len[MSG_MAX];
static int    head, tail, count;

/* write (under the mutex) */
if (count == MSG_MAX) { ret = -ENOSPC; goto out; }
n = min_t(size_t, len, MSG_LEN);
if (copy_from_user(msgs[tail], buf, n)) { ret = -EFAULT; goto out; }
msg_len[tail] = n;
tail = (tail + 1) % MSG_MAX;
count++;
ret = len;

/* read (under the mutex) – one message per open */
if (*off > 0 || count == 0) { ret = 0; goto out; }
n = min_t(size_t, len, msg_len[head]);
if (copy_to_user(buf, msgs[head], n)) { ret = -EFAULT; goto out; }
head = (head + 1) % MSG_MAX;
count--;
*off += n;
ret = n;
```

**Test in QEMU**

```sh
echo a > /dev/lab2 ; echo b > /dev/lab2
cat /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2      # a, b, (empty)
```

**Pitfalls**

- Not checking *off > 0 → cat also takes the second message on its next read
- Forgetting % MSG_MAX → index runs past the array

### Exercise 18. BLOCKING read (wait queue) ★★★

**Task:** When the buffer is empty, cat waits until someone writes.

**Header:** `<linux/wait.h>` · **Main functions:** `DECLARE_WAIT_QUEUE_HEAD`, `wait_event_interruptible`, `wake_up_interruptible`

| Region | What to do |
|---|---|
| ⓪ | #include <linux/wait.h>; DECLARE_WAIT_QUEUE_HEAD(lab2_wq) |
| Ⓐ read | wait_event_interruptible BEFORE taking the mutex |
| Ⓐ write | wake_up_interruptible AFTER releasing the mutex |

**Core code**

```c
static DECLARE_WAIT_QUEUE_HEAD(lab2_wq);

/* start of lab2_read */
if (wait_event_interruptible(lab2_wq, buffer_len > 0))
    return -ERESTARTSYS;            /* Ctrl+C while waiting */
mutex_lock(&lab2_mutex);
/* … read logic as before … */

/* end of lab2_write, after mutex_unlock */
wake_up_interruptible(&lab2_wq);
```

**Test in QEMU**

```sh
# right after boot the buffer is empty
cat /dev/lab2 &            # waits
echo hi > /dev/lab2        # cat prints "hi"
```

**Pitfalls**

- Waiting while holding the mutex → write cannot take the lock to wake you (deadlock)
- Forgetting wake_up → cat waits forever
- Wrong condition (e.g. buffer_len == 0) → never blocks

### Exercise 19. ioctl commands: CLEAR and GET_LEN ★★★

**Task:** LAB2_CLEAR empties the buffer; LAB2_GET_LEN returns buffer_len to userspace.

**Header:** `<linux/ioctl.h> / <sys/ioctl.h>`, `<linux/uaccess.h>` · **Main functions:** `_IO`, `_IOR`, `_IOC_TYPE`, `put_user`

| Region | What to do |
|---|---|
| shared header | Magic + two _IO/_IOR commands |
| Ⓐ ioctl | unlocked_ioctl handler + add it to fops |
| userspace | C program calling ioctl, built with -static |

**Core code**

```c
#define LAB2_MAGIC    'L'
#define LAB2_CLEAR    _IO (LAB2_MAGIC, 0)
#define LAB2_GET_LEN  _IOR(LAB2_MAGIC, 1, int)

static long lab2_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    int v;
    if (_IOC_TYPE(cmd) != LAB2_MAGIC) return -ENOTTY;
    switch (cmd) {
    case LAB2_CLEAR:
        mutex_lock(&lab2_mutex); buffer_len = 0; mutex_unlock(&lab2_mutex);
        return 0;
    case LAB2_GET_LEN:
        mutex_lock(&lab2_mutex); v = buffer_len; mutex_unlock(&lab2_mutex);
        return put_user(v, (int __user *)arg);
    default:
        return -ENOTTY;
    }
}
/* fops: .unlocked_ioctl = lab2_ioctl, */
```

**Test in QEMU**

```sh
arm-linux-gnueabihf-gcc -static -Wall -o lab2_app lab2_app.c      # on the laptop
lab2_app getlen ; lab2_app clear ; cat /dev/lab2                     # in QEMU
```

**Pitfalls**

- Forgetting -static → not found
- Forgetting .unlocked_ioctl → Inappropriate ioctl for device
- Different headers on each side → command codes mismatch

> Full solution: [`ASSIGNMENT1/task1_ioctl`](../ASSIGNMENT1/task1_ioctl/)

### Exercise 20. Two devices /dev/lab2_0 and /dev/lab2_1 ★★★

**Task:** One driver, two minors, each device with its own buffer.

**Header:** `<linux/fs.h>`, `<linux/device.h>`, `<linux/mutex.h>` · **Main functions:** `iminor`, `file->private_data`, `mutex_init`, `device_create`

| Region | What to do |
|---|---|
| ⓪ | struct lab2_dev { buf, len, mutex } devs[2] |
| ⓘ init | mutex_init for each dev; device_create twice (minor 0, 1) |
| Ⓐ open | iminor(inode) → file->private_data = &devs[minor] |
| Ⓐ read/write | Use file->private_data instead of globals |

**Core code**

```c
#define NUM_DEV 2
struct lab2_dev { char buf[BUFFER_SIZE]; int len; struct mutex lock; };
static struct lab2_dev devs[NUM_DEV];

static int lab2_open(struct inode *inode, struct file *file)
{
    unsigned int minor = iminor(inode);
    if (minor >= NUM_DEV) return -ENODEV;
    file->private_data = &devs[minor];
    return 0;
}
/* read/write: struct lab2_dev *d = file->private_data; use d->buf, d->len, d->lock */

/* init */
for (i = 0; i < NUM_DEV; i++) {
    mutex_init(&devs[i].lock);
    device_create(lab2_class, NULL, MKDEV(major_number, i), NULL, "lab2_%d", i);
}
/* exit: device_destroy(lab2_class, MKDEV(major_number, i)) for each i */
```

**Test in QEMU**

```sh
echo A > /dev/lab2_0 ; echo B > /dev/lab2_1
cat /dev/lab2_0 ; cat /dev/lab2_1        # A  then  B
```

**Pitfalls**

- Forgetting mutex_init for a mutex inside a struct
- Still using the old globals in read/write
- rcS / test_driver.sh still look for /dev/lab2 → must be updated

## 6. Variants

| # | Exercise | Difficulty |
|---|---|---|
| B1 | ROT13 encoding | ★★☆ |
| B2 | Swap case | ★★☆ |
| B3 | Read-only switch (sysfs readonly) | ★★☆ |
| B4 | Count words and lines | ★★☆ |
| B5 | Dynamic buffer size (kmalloc) | ★★★ |
| B6 | Convert to a misc device | ★★☆ |
| B7 | Driver uptime | ★★☆ |
| B8 | lseek support (read from any position) | ★★☆ |

### B1. ROT13 encoding ★★☆

**Task:** Store data ROT13-encoded (shift each letter 13 places, keep case).

**Header:** `<linux/ctype.h>` · **Main functions:** `isalpha`, `islower`

| Region | What to do |
|---|---|
| Ⓐ write | Loop after copy_from_user |

**Core code**

```c
for (i = 0; i < len; i++) {
    char c = device_buffer[i];
    if (isalpha(c)) {
        char base = islower(c) ? 'a' : 'A';
        device_buffer[i] = base + (c - base + 13) % 26;
    }
}
```

**Test in QEMU**

```sh
echo Hello > /dev/lab2 ; cat /dev/lab2      # Uryyb
```

**Pitfalls**

- Forgetting % 26 → characters outside the alphabet

### B2. Swap case ★★☆

**Task:** Upper case becomes lower case and vice versa.

**Header:** `<linux/ctype.h>` · **Main functions:** `isupper`, `islower`, `toupper`, `tolower`

| Region | What to do |
|---|---|
| Ⓐ write | Loop after copy_from_user |

**Core code**

```c
for (i = 0; i < len; i++) {
    char c = device_buffer[i];
    if (isupper(c))      device_buffer[i] = tolower(c);
    else if (islower(c)) device_buffer[i] = toupper(c);
}
```

**Test in QEMU**

```sh
echo HeLLo > /dev/lab2 ; cat /dev/lab2      # hEllO
```

**Pitfalls**

- Two separate ifs → a letter just changed gets changed back

### B3. Read-only switch (sysfs readonly) ★★☆

**Task:** Attribute readonly (0/1); when 1, write returns -EPERM.

**Header:** `<linux/device.h>`, `<linux/kernel.h>` · **Main functions:** `DEVICE_ATTR_RW`, `kstrtoint`

| Region | What to do |
|---|---|
| ⓪ | static int readonly |
| Ⓑ sysfs | readonly_show/store + DEVICE_ATTR_RW |
| Ⓐ write | Check at the start (under the mutex) |

**Core code**

```c
/* start of lab2_write, after mutex_lock */
if (readonly) { ret = -EPERM; goto out; }
```

**Test in QEMU**

```sh
echo 1 > /sys/class/lab2_class/lab2/readonly
echo hi > /dev/lab2          # Operation not permitted
```

**Pitfalls**

- _store accepting values other than 0/1

### B4. Count words and lines ★★☆

**Task:** /proc/lab2_info shows the number of words (runs of non-space) and lines ('\n').

**Header:** `<linux/ctype.h>` · **Main functions:** `isspace`

| Region | What to do |
|---|---|
| Ⓑ proc | Walk the buffer, count space → non-space transitions |

**Core code**

```c
int i, words = 0, lines = 0, in_word = 0;
for (i = 0; i < buffer_len; i++) {
    char c = device_buffer[i];
    if (c == '\n') lines++;
    if (isspace(c)) in_word = 0;
    else if (!in_word) { in_word = 1; words++; }
}
seq_printf(m, "Words / Lines : %d / %d\n", words, lines);
```

**Test in QEMU**

```sh
printf 'one two\nthree\n' > /dev/lab2 ; grep Words /proc/lab2_info    # 3 / 2
```

**Pitfalls**

- Counting spaces instead of word starts

### B5. Dynamic buffer size (kmalloc) ★★★

**Task:** Parameter buf_size (64–4096); the buffer is allocated with kzalloc in init.

**Header:** `<linux/slab.h>` · **Main functions:** `kzalloc`, `kfree`

| Region | What to do |
|---|---|
| ⓪ | static char *device_buffer; static int buf_size = 1024; module_param |
| ⓘ init | kzalloc BEFORE register_chrdev; failure → -ENOMEM |
| ⓘ exit | kfree LAST |
| Everywhere | Replace BUFFER_SIZE with buf_size |

**Core code**

```c
device_buffer = kzalloc(buf_size, GFP_KERNEL);
if (!device_buffer)
    return -ENOMEM;
/* register_chrdev … if a later step fails: remember kfree(device_buffer) */

/* lab2_exit, after unregister_chrdev */
kfree(device_buffer);
```

**Test in QEMU**

```sh
rmmod lab2_driver ; insmod /lib/modules/5.15.221/lab2_driver.ko buf_size=64
head -c 200 /dev/zero | tr '\0' x > /dev/lab2 ; grep 'Data length' /proc/lab2_info   # 63
```

**Pitfalls**

- sizeof(device_buffer) is now 4 (a pointer), not the buffer size
- Forgetting kfree on init error paths → memory leak

### B6. Convert to a misc device ★★☆

**Task:** Replace register_chrdev + class + device with a miscdevice; keep procfs and sysfs.

**Header:** `<linux/miscdevice.h>` · **Main functions:** `misc_register`, `misc_deregister`, `ATTRIBUTE_GROUPS`

| Region | What to do |
|---|---|
| ⓘ init/exit | misc_register / misc_deregister |
| Ⓑ sysfs | Use the miscdevice .groups field |

**Core code**

```c
ATTRIBUTE_GROUPS(lab2);               /* builds lab2_groups from lab2_attrs[] */

static struct miscdevice lab2_misc = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "lab2",                  /* → /dev/lab2, major 10 */
    .fops   = &lab2_fops,
    .groups = lab2_groups,             /* sysfs: /sys/class/misc/lab2/… */
    .mode   = 0666,
};
/* init: ret = misc_register(&lab2_misc);   exit: misc_deregister(&lab2_misc); */
```

**Test in QEMU**

```sh
ls -l /dev/lab2                         # 10, <minor>
cat /sys/class/misc/lab2/buffer_len
```

**Pitfalls**

- Still calling sysfs_create_group on lab2_device (no longer exists)
- The sysfs path changes to /sys/class/misc/

### B7. Driver uptime ★★☆

**Task:** /proc/lab2_info shows the seconds since insmod.

**Header:** `<linux/jiffies.h>` · **Main functions:** `jiffies`, `jiffies_to_msecs`

| Region | What to do |
|---|---|
| ⓪ | static unsigned long load_jiffies |
| ⓘ init | load_jiffies = jiffies |
| Ⓑ proc | jiffies_to_msecs(jiffies - load_jiffies) / 1000 |

**Core code**

```c
seq_printf(m, "Uptime        : %u s\n",
           jiffies_to_msecs(jiffies - load_jiffies) / 1000);
```

**Test in QEMU**

```sh
sleep 5 ; grep Uptime /proc/lab2_info
```

**Pitfalls**

- Dividing jiffies by 100 – wrong if HZ differs (use jiffies_to_msecs)

### B8. lseek support (read from any position) ★★☆

**Task:** Allow dd skip= / lseek to change the read position.

**Header:** `<linux/fs.h>` · **Main functions:** `default_llseek`

| Region | What to do |
|---|---|
| Ⓐ fops | .llseek = default_llseek |

**Core code**

```c
static struct file_operations lab2_fops = {
    /* … */
    .llseek = default_llseek,
};
```

**Test in QEMU**

```sh
echo "Hello World" > /dev/lab2
dd if=/dev/lab2 bs=1 skip=6 2>/dev/null       # World
```

**Pitfalls**

- read does not start from *offset → seeking has no effect

## 7. Required names

| Required name | Change | Visible in |
|---|---|---|
| Module | The `.c` file name **and** the Makefile `obj-m` line | `lsmod`, `insmod X.ko`, `rmmod X` |
| Device | `#define DEVICE_NAME` | `/dev/…`, `/proc/devices` |
| Class | `#define CLASS_NAME` | `/sys/class/…` |
| proc file | String in `proc_create("…", …)` | `/proc/…` |
| Major | `#define DRIVER_MAJOR` (241–254) | `ls -l /dev/…` |

> ⚠️ `rcS` always loads `lab2_driver.ko`, which takes major 240 → a renamed driver still using 240 fails with `Device or resource busy`. A renamed driver is not loaded by `rcS` – `insmod` it yourself.

```bash
mv lab2_driver.c mydev.c
sed -i 's/lab2_driver\.o/mydev.o/' Makefile
# edit by hand: DEVICE_NAME, CLASS_NAME, "lab2_info", DRIVER_MAJOR
grep -n '"lab2\|240\|lab2_info' mydev.c      # must print nothing
```

Example: [`ex02b_rename`](ex02b_rename/)

## 8. Error codes

| Code | Message the user sees | Returned when |
|---|---|---|
| `-EFAULT` | Bad address | copy_*_user failed |
| `-EINVAL` | Invalid argument | Bad parameter (out of range, not a number) |
| `-EBUSY` | Device or resource busy | Device already in use |
| `-ENOSPC` | No space left on device | Buffer / queue full |
| `-EPERM` | Operation not permitted | Operation not allowed (e.g. read-only mode) |
| `-ENOTTY` | Inappropriate ioctl for device | Unsupported ioctl command |
| `-ENOMEM` | Out of memory | kmalloc returned NULL |
| `-EAGAIN` | Resource temporarily unavailable | No data yet, non-blocking open |
| `-ERESTARTSYS` | (interrupted by a signal) | wait_event_interruptible / mutex_lock_interruptible failed |
| `-ENODEV` | No such device | Minor does not exist |

## 9. Checklist before calling the examiner

- [ ] `make` prints no warnings.
- [ ] Every `return` after `mutex_lock` goes through `mutex_unlock`.
- [ ] User ↔ kernel data only via `copy_to_user` / `copy_from_user` (not needed for the sysfs `_store` buf).
- [ ] `read` advances `*offset` and returns `len` (not `*offset`).
- [ ] `write` / `_store` return the bytes consumed (`len` / `count`).
- [ ] Test boundary values (0, 1, middle, max, max+1) for any range.
- [ ] Everything created in init is destroyed in exit, in reverse order; `rmmod` works.
- [ ] Log saved with `script`; you can explain echo → syscall → VFS → fops → driver.
