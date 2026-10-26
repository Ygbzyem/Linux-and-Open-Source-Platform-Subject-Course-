# 05 – Kernel API cheat sheet (kernel 5.15)

**English** | [Tiếng Việt](vi/05-kernel-api.md) · [← Home](../README.md)

Organised by **situation**. Each entry: header to include → signature → short example.

> Verify any signature against the real source on your machine (no internet needed):
> ```bash
> K=~/embedded_lab1/kernel/linux-5.15.221
> grep -rn 'int misc_register' $K/include/
> grep -rn 'define DEVICE_ATTR_RW' $K/include/
> ```

---

## Contents

1. [Copying data between user and kernel](#1-copying-data-between-user-and-kernel)
2. [Locking and synchronisation](#2-locking-and-synchronisation)
3. [Registering a character device](#3-registering-a-character-device)
4. [Misc device](#4-misc-device)
5. [file_operations signatures](#5-file_operations-signatures)
6. [procfs](#6-procfs)
7. [sysfs](#7-sysfs)
8. [module_param](#8-module_param)
9. [ioctl](#9-ioctl)
10. [Wait queues – block until data is available](#10-wait-queues--block-until-data-is-available)
11. [Strings, numbers, characters](#11-strings-numbers-characters)
12. [Time, randomness, current process](#12-time-randomness-current-process)
13. [Memory allocation](#13-memory-allocation)
14. [Logging](#14-logging)
15. [Common error codes](#15-common-error-codes)

---

## 1. Copying data between user and kernel

```c
#include <linux/uaccess.h>

unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
int get_user(x, ptr);          /* a single scalar (int, char…) from user space */
int put_user(x, ptr);          /* a single scalar to user space */
```

- Returns the **number of bytes NOT copied** → non-zero means failure → `return -EFAULT`.
- Kernel → kernel (both sides are kernel memory): plain `memcpy` is fine.

```c
if (copy_from_user(kbuf, ubuf, n))
    return -EFAULT;
```

---

## 2. Locking and synchronisation

### Mutex (may sleep – use in read/write/ioctl)

```c
#include <linux/mutex.h>
static DEFINE_MUTEX(my_lock);

mutex_lock(&my_lock);
mutex_unlock(&my_lock);
int mutex_trylock(&my_lock);              /* 1 = acquired, 0 = busy (does not wait) */
int mutex_lock_interruptible(&my_lock);   /* 0 = OK, -EINTR if a signal arrives (Ctrl+C) */
```

```c
if (mutex_lock_interruptible(&my_lock))
    return -ERESTARTSYS;
```

### Atomics (a single counter, no mutex needed)

```c
#include <linux/atomic.h>
static atomic_t counter = ATOMIC_INIT(0);

atomic_inc(&counter);           atomic_dec(&counter);
atomic_set(&counter, 5);        int v = atomic_read(&counter);
int old = atomic_cmpxchg(&counter, 0, 1);   /* if ==0 set to 1; returns the OLD value */
int r   = atomic_inc_return(&counter);
```

Single-open with an atomic:

```c
static atomic_t in_use = ATOMIC_INIT(0);
/* open */    if (atomic_cmpxchg(&in_use, 0, 1) != 0) return -EBUSY;
/* release */ atomic_set(&in_use, 0);
```

| Use | When |
|---|---|
| `atomic_t` | A single counter / flag |
| `mutex` | Several variables that must change together (buffer + len), or code that may sleep |
| `spinlock` | Interrupt context / code that must not sleep (not used in this course yet) |

---

## 3. Registering a character device

```c
#include <linux/fs.h>
#include <linux/device.h>

int  register_chrdev(unsigned int major, const char *name, const struct file_operations *fops);
void unregister_chrdev(unsigned int major, const char *name);

struct class  *class_create(struct module *owner, const char *name);   /* 5.15: 2 arguments */
void           class_destroy(struct class *cls);

struct device *device_create(struct class *cls, struct device *parent,
                             dev_t devt, void *drvdata, const char *fmt, ...);
void           device_destroy(struct class *cls, dev_t devt);

dev_t MKDEV(major, minor);   MAJOR(dev);   MINOR(dev);
unsigned int iminor(const struct inode *inode);    /* minor of the file being opened */
```

**Dynamic major:**

```c
static int major;
major = register_chrdev(0, NAME, &fops);   /* 0 → kernel picks one, returns it */
if (major < 0) return major;
```

---

## 4. Misc device

Major is always **10**, the kernel assigns the minor and creates `/dev/<name>`. No need for
`register_chrdev` + `class_create` + `device_create`.

```c
#include <linux/miscdevice.h>

static struct miscdevice my_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "counter",            /* → /dev/counter */
    .fops  = &my_fops,
    .mode  = 0666,                 /* permissions of /dev/counter */
};

/* init */ int ret = misc_register(&my_misc);   if (ret) return ret;
/* exit */ misc_deregister(&my_misc);
```

Check: `cat /proc/misc` shows `<minor> counter`; `ls -l /dev/counter` → `crw-… 10, <minor>`.

---

## 5. file_operations signatures

```c
static int     my_open   (struct inode *inode, struct file *file);
static int     my_release(struct inode *inode, struct file *file);
static ssize_t my_read   (struct file *file, char __user *buf, size_t len, loff_t *off);
static ssize_t my_write  (struct file *file, const char __user *buf, size_t len, loff_t *off);
static long    my_ioctl  (struct file *file, unsigned int cmd, unsigned long arg);

static const struct file_operations my_fops = {
    .owner          = THIS_MODULE,
    .open           = my_open,
    .release        = my_release,
    .read           = my_read,
    .write          = my_write,
    .unlocked_ioctl = my_ioctl,
    .llseek         = default_llseek,      /* if you need lseek / dd seek= */
};
```

`file->private_data`: a per-**open** pointer – set it in `open`, use it in `read/write`
(e.g. a driver with several minors, one buffer each).

---

## 6. procfs

```c
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int my_show(struct seq_file *m, void *v)
{
    seq_printf(m, "count : %d\n", count);
    return 0;
}
static int my_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, my_show, NULL);
}
static const struct proc_ops my_proc_ops = {
    .proc_open    = my_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static struct proc_dir_entry *pe;
/* init */ pe = proc_create("my_info", 0444, NULL, &my_proc_ops);
/* exit */ proc_remove(pe);
```

> Kernels 5.6+ use `struct proc_ops`. Older tutorials use `struct file_operations` for procfs –
> that does not build on 5.15.

---

## 7. sysfs

```c
#include <linux/device.h>
#include <linux/sysfs.h>

/* Fixed signatures */
static ssize_t xxx_show (struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t xxx_store(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count);

static DEVICE_ATTR_RO(xxx);   /* 0444 – needs xxx_show */
static DEVICE_ATTR_RW(xxx);   /* 0644 – needs xxx_show + xxx_store */
static DEVICE_ATTR_WO(xxx);   /* 0200 – needs xxx_store */
/* → creates the variable dev_attr_xxx */
```

Writable attribute example:

```c
static int mode;
static ssize_t mode_show(struct device *d, struct device_attribute *a, char *buf)
{
    return sprintf(buf, "%d\n", mode);
}
static ssize_t mode_store(struct device *d, struct device_attribute *a,
                          const char *buf, size_t count)
{
    int v;
    if (kstrtoint(buf, 10, &v) || v < 0 || v > 2)
        return -EINVAL;
    mode = v;
    return count;            /* MUST return count, otherwise echo keeps writing */
}
static DEVICE_ATTR_RW(mode);
```

Registration:

```c
/* Option 1 – add to the existing attrs array, then sysfs_create_group (as in lab2_driver.c) */
&dev_attr_mode.attr,

/* Option 2 – one file at a time */
device_create_file(my_device, &dev_attr_mode);      /* init */
device_remove_file(my_device, &dev_attr_mode);      /* exit, BEFORE device_destroy */
```

---

## 8. module_param

```c
#include <linux/moduleparam.h>

static int max_len = 1024;
module_param(max_len, int, 0444);           /* types: int, uint, bool, charp, ulong… */
MODULE_PARM_DESC(max_len, "Max bytes stored per write");
```

```sh
insmod my.ko max_len=5
cat /sys/module/my/parameters/max_len
```

Permission `0444` → read-only; `0644` → root may change it while the module runs (then think
about synchronisation). Validate the value in `init` and `return -EINVAL` if invalid.

---

## 9. ioctl

**Shared header** for kernel and user space:

```c
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define MY_MAGIC   0xA5
#define MY_RESET   _IO (MY_MAGIC, 0)                 /* no data */
#define MY_GET     _IOR(MY_MAGIC, 1, struct my_stats) /* kernel → user */
#define MY_SET     _IOW(MY_MAGIC, 2, int)             /* user → kernel */
```

32-bit command layout:

```text
 31 30 │ 29 ……… 16 │ 15 …… 8 │ 7 …… 0
  DIR  │   SIZE    │  TYPE   │   NR
 0=none 1=W 2=R    sizeof(t)   magic    sequence number
```

```c
_IOC_DIR(cmd)  _IOC_SIZE(cmd)  _IOC_TYPE(cmd)  _IOC_NR(cmd)
```

**Kernel side:**

```c
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    void __user *uarg = (void __user *)arg;
    int v;

    if (_IOC_TYPE(cmd) != MY_MAGIC) return -ENOTTY;
    switch (cmd) {
    case MY_SET:
        if (copy_from_user(&v, uarg, sizeof(v))) return -EFAULT;
        /* … */
        return 0;
    default:
        return -ENOTTY;
    }
}
```

**User side** (the shell cannot call ioctl → you need a C program):

```c
int fd = open("/dev/mydev", O_RDWR);
int v = 1;
if (ioctl(fd, MY_SET, &v) < 0) perror("ioctl");
```

Build: `arm-linux-gnueabihf-gcc -static -Wall -o app app.c` (**`-static` is mandatory**).

Full example: [`ASSIGNMENT1/task1_ioctl/`](../ASSIGNMENT1/task1_ioctl/).

---

## 10. Wait queues – block until data is available

```c
#include <linux/wait.h>
static DECLARE_WAIT_QUEUE_HEAD(my_wq);

/* read: sleep until the condition is true; -ERESTARTSYS on Ctrl+C */
if (wait_event_interruptible(my_wq, buffer_len > 0))
    return -ERESTARTSYS;

/* write: after new data arrives */
wake_up_interruptible(&my_wq);
```

> Never call `wait_event_*` while holding the mutex – the writer could not take the lock to wake
> you up. Correct order: wait → take the lock → re-check the condition.

---

## 11. Strings, numbers, characters

```c
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/ctype.h>

size_t  strlen(const char *s);
ssize_t strscpy(char *dst, const char *src, size_t size);   /* always NUL-terminates, safe */
int     snprintf(char *buf, size_t size, const char *fmt, ...);
int     sprintf(char *buf, const char *fmt, ...);           /* only in sysfs show */

int kstrtoint (const char *s, unsigned int base, int *res);        /* 0 = OK, <0 = error */
int kstrtouint(const char *s, unsigned int base, unsigned int *res);
int kstrtol   (const char *s, unsigned int base, long *res);

min(a, b)   max(a, b)   min_t(size_t, a, b)   clamp(v, lo, hi)

toupper(c) tolower(c) isalpha(c) isdigit(c) isalnum(c) isspace(c) isprint(c)
```

`kstrtoint` accepts a trailing `\n` (added by `echo`) – no need to strip it yourself.

> ❗ **No `float`/`double` in the kernel.** Use integers in a smaller unit (e.g. 0.1 °C):
> `seq_printf(m, "%d.%d", t / 10, t % 10);`

---

## 12. Time, randomness, current process

```c
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/sched.h>

u64    ktime_get_ns(void);                    /* ns since boot (monotonic) */
s64    ktime_to_ms(ktime_t kt);               /* ktime_to_ms(ktime_get()) → ms */
time64_t ktime_get_real_seconds(void);        /* wall-clock seconds */
unsigned long jiffies;                        /* ticks since boot (HZ=100 → 10 ms/tick) */
unsigned int jiffies_to_msecs(unsigned long j);

u32 get_random_u32(void);                     /* 32-bit random number */
/* range [lo, hi]:  lo + get_random_u32() % (hi - lo + 1) */

current->pid      current->comm               /* PID and name of the calling process */
```

---

## 13. Memory allocation

```c
#include <linux/slab.h>

void *kmalloc(size_t size, gfp_t flags);      /* GFP_KERNEL where sleeping is allowed */
void *kzalloc(size_t size, gfp_t flags);      /* like kmalloc but zeroed */
void  kfree(const void *p);
```

Every `kmalloc` in `init` needs a matching `kfree` in `exit` (and on every error path of `init`).

---

## 14. Logging

```c
pr_info("mydrv: value=%d\n", v);
pr_warn(...);   pr_err(...);   pr_debug(...);   /* pr_debug needs dynamic debug enabled */
```

| Format | Type |
|---|---|
| `%d` / `%u` | `int` / `unsigned int` |
| `%ld` / `%lld` | `long` / `long long` (`s64`) |
| `%zu` / `%zd` | `size_t` / `ssize_t` |
| `%lu` | `unsigned long` (`jiffies`) |
| `%llu` | `u64` |
| `%#x` | hex with `0x` |
| `%s` / `%c` | string / char |

Read logs: `dmesg | tail -20`, `dmesg | grep mydrv`.

---

## 15. Common error codes

| Code | Message | Return it when |
|---|---|---|
| `-EFAULT` | Bad address | `copy_*_user` failed |
| `-EINVAL` | Invalid argument | Bad parameter (mode out of range, negative number…) |
| `-ENOTTY` | Inappropriate ioctl for device | Unsupported ioctl command |
| `-EBUSY` | Device or resource busy | Device already open / in use |
| `-ENOMEM` | Out of memory | `kmalloc` returned `NULL` |
| `-ENOSPC` | No space left on device | Buffer/queue full |
| `-EAGAIN` | Try again | Not ready yet (non-blocking, read too soon) |
| `-ERESTARTSYS` | (interrupted by a signal) | `wait_event_interruptible` / `mutex_lock_interruptible` failed |
| `-ENODEV` | No such device | Hardware missing |

User space sees these through `errno`; `perror("ioctl")` prints the "Message" column.

---

**Next:** [06 – Troubleshooting](06-troubleshooting.md)
