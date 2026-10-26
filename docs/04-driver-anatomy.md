# 04 – Anatomy of a character driver

**English** | [Tiếng Việt](vi/04-giai-phau-driver.md) · [← Home](../README.md)

Based on the real driver in this repo: [`LAB2/embedded_lab2/driver/lab2_driver.c`](../LAB2/embedded_lab2/driver/lab2_driver.c).

---

## 1. The three layers of a driver

```text
┌──────────────────────────────────────────────────────────────────┐
│ LAYER 1 – REGISTRATION (init / exit)    rarely changes between tasks │
│   register_chrdev → class_create → device_create → proc/sysfs     │
├──────────────────────────────────────────────────────────────────┤
│ LAYER 2 – OPERATIONS (file_operations)  EXAM TASKS CHANGE THIS     │
│   open / read / write / release / unlocked_ioctl                  │
├──────────────────────────────────────────────────────────────────┤
│ LAYER 3 – EXPOSING INFORMATION          depends on the task        │
│   procfs (/proc/…), sysfs (/sys/class/…), module_param            │
└──────────────────────────────────────────────────────────────────┘
```

ESP32 analogy: layer 1 ≈ `setup()` + attaching interrupts, layer 2 ≈ callbacks, layer 3 ≈
printing status over Serial.

---

## 2. File map, top to bottom

```text
 ⓪ DATA            #include, MODULE_*, #define, static variables, DEFINE_MUTEX
 Ⓐ OPERATIONS      lab2_open, lab2_release, lab2_read, lab2_write, lab2_fops
 Ⓑ EXPOSING INFO   lab2_proc_show + proc_ops   │  *_show + DEVICE_ATTR_RO + attr_group
 ⓘ REGISTRATION    lab2_init, lab2_exit, module_init/module_exit
```

> C is read top-down: a function must be defined **before** it is used. That is why
> `lab2_fops` comes after the four Ⓐ functions and `lab2_init` comes last.

Quick map of any driver file:

```bash
grep -n '/\* ===\|^static.*(\|module_init\|module_exit\|DEVICE_ATTR' lab2_driver.c
```

---

## 3. Region ⓪ – data

```c
MODULE_LICENSE("GPL");                 // missing → kernel "tainted", some APIs unavailable
#define DEVICE_NAME  "lab2"            // name in /proc/devices and /dev/lab2
#define CLASS_NAME   "lab2_class"      // /sys/class/lab2_class/
#define BUFFER_SIZE  1024
#define DRIVER_MAJOR 240

static char   device_buffer[BUFFER_SIZE];   // the device's "memory"
static int    buffer_len = 0;
static int    open_count = 0;
static DEFINE_MUTEX(lab2_mutex);            // protects the three variables above
```

- File-scope `static` → visible only in this file, lives from `insmod` to `rmmod`.
- Every variable that **several processes read/write** must be protected by the mutex.

---

## 4. Region Ⓐ – from system call to our function

```text
 User space:  echo hello > /dev/lab2
     │  open("/dev/lab2")              ── system call
     ▼
 VFS: inode is a char device, major 240 ──► looks up what register_chrdev registered
     │
     ▼
 lab2_fops.open    = lab2_open        ← open_count++
     │  write(fd, "hello\n", 6)
     ▼
 lab2_fops.write   = lab2_write       ← copy_from_user → device_buffer
     │  close(fd)
     ▼
 lab2_fops.release = lab2_release
```

**The major number is how VFS knows which driver to call. `file_operations` is the table of
function pointers the driver hands to the kernel.**

### 4.1. `read` – four steps

```c
static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    if (*offset >= buffer_len)                 // ① no data left → return 0 = EOF
        return 0;
    if (len > buffer_len - *offset)            // ② clamp to what remains
        len = buffer_len - *offset;
    if (copy_to_user(buf, device_buffer + *offset, len))   // ③ copy to user
        return -EFAULT;
    *offset += len;                            // ④ advance for the next call
    return len;
}
```

Why step ④ matters: `cat` keeps calling `read()` until it gets **0**. Without updating
`*offset`, every call returns data from the start again → `cat` never stops.

```text
cat /dev/lab2   (buffer = "hello\n", 6 bytes)
  read(fd, buf, 4096)  offset=0  → returns 6, offset=6   → prints "hello"
  read(fd, buf, 4096)  offset=6  → returns 0             → cat stops
```

> ⚠️ The original LAB-02 `read` does **not** take the mutex. The ASSIGNMENT-01 version
> ([`asgn1_driver.c`](../ASSIGNMENT1/task1_ioctl/asgn1_driver.c)) fixes this with the `goto out` pattern.

### 4.2. `write`

```c
mutex_lock(&lab2_mutex);
if (copy_from_user(device_buffer, buf, len)) { mutex_unlock(&lab2_mutex); return -EFAULT; }
buffer_len = len;
device_buffer[buffer_len] = '\0';
mutex_unlock(&lab2_mutex);
return len;
```

`write` must return the **number of bytes consumed**. Returning less than `len` makes user
space think it was a partial write and **call again** with the rest (see the lesson in
ASSIGNMENT-01 CP4).

### 4.3. Single-exit pattern – for functions with several error paths

```c
ssize_t ret;
mutex_lock(&m);
if (error_condition_1) { ret = 0;       goto out; }
if (error_condition_2) { ret = -EFAULT; goto out; }
/* normal path */
ret = len;
out:
mutex_unlock(&m);       // written ONCE → impossible to forget
return ret;
```

Rule of thumb: **no `return` between `mutex_lock` and `out:`.**

---

## 5. Region Ⓑ – exposing information

### 5.1. procfs: `/proc/lab2_info`

```text
cat /proc/lab2_info
   → proc_open    = lab2_proc_open → single_open(file, lab2_proc_show, NULL)
   → proc_read    = seq_read       → calls lab2_proc_show() to generate the text
   → proc_release = single_release
```

You only write `lab2_proc_show` with `seq_printf` calls; `seq_file` handles buffering, offsets
and EOF.

### 5.2. sysfs: `/sys/class/lab2_class/lab2/*`

```c
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", buffer_len);
}
static DEVICE_ATTR_RO(buffer_len);        // creates dev_attr_buffer_len, mode 0444
```

Naming rule: `DEVICE_ATTR_RO(xxx)` **requires** a function named exactly `xxx_show`;
`_RW` also needs `xxx_store`; `_WO` only needs `xxx_store`.

Group them and register once:

```c
static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr, &dev_attr_open_count.attr, &dev_attr_last_data.attr, NULL,
};
static struct attribute_group lab2_attr_group = { .attrs = lab2_attrs };
/* in init: */ sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
```

| | procfs | sysfs |
|---|---|---|
| Format | One multi-line text file | One value per file |
| Intended reader | Humans | Programs, scripts |
| Writable? | Possible, rarely used | Yes (`_RW`, `_WO`) – the standard way to configure a driver |

---

## 6. Region ⓘ – registration and teardown

```text
 lab2_init                                lab2_exit (REVERSE ORDER)
 ① register_chrdev(240, "lab2", &fops)    ⑤ proc_remove
 ② class_create("lab2_class")             ④ sysfs_remove_group
 ③ device_create(…, MKDEV(240,0), "lab2") ③ device_destroy
 ④ sysfs_create_group                     ② class_destroy
 ⑤ proc_create("lab2_info")               ① unregister_chrdev
```

| Call | Visible result |
|---|---|
| `register_chrdev` | Line `240 lab2` in `/proc/devices` |
| `class_create` | Directory `/sys/class/lab2_class/` |
| `device_create` | `/sys/class/lab2_class/lab2/` **and devtmpfs creates `/dev/lab2` automatically** |
| `proc_create` | `/proc/lab2_info` |

**Error handling in `init`:** if a step fails, undo every step that **already succeeded**, then
return the error code – otherwise the next `insmod` fails with `Device or resource busy`
because major 240 is still taken.

`IS_ERR` / `PTR_ERR`: functions returning a pointer encode a negative error code inside the
pointer itself.

```c
cls = class_create(THIS_MODULE, CLASS_NAME);
if (IS_ERR(cls)) return PTR_ERR(cls);
```

> `class_create(THIS_MODULE, name)` is the kernel 5.15 signature. Since 6.4 it takes a single
> argument: `class_create(name)`.

---

## 7. Minimal skeleton – you should be able to type it from memory

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");

#define NAME     "mydev"
#define MAJ      241
#define BUF_SIZE 1024

static char buf[BUF_SIZE];
static int  len;
static struct class *cls;
static DEFINE_MUTEX(lock);

static int my_open(struct inode *i, struct file *f)    { return 0; }
static int my_release(struct inode *i, struct file *f) { return 0; }

static ssize_t my_read(struct file *f, char __user *ubuf, size_t n, loff_t *off)
{
    ssize_t ret;
    mutex_lock(&lock);
    if (*off >= len) { ret = 0; goto out; }
    if (n > len - *off) n = len - *off;
    if (copy_to_user(ubuf, buf + *off, n)) { ret = -EFAULT; goto out; }
    *off += n;
    ret = n;
out:
    mutex_unlock(&lock);
    return ret;
}

static ssize_t my_write(struct file *f, const char __user *ubuf, size_t n, loff_t *off)
{
    ssize_t ret;
    size_t c = min_t(size_t, n, BUF_SIZE - 1);
    mutex_lock(&lock);
    if (copy_from_user(buf, ubuf, c)) { ret = -EFAULT; goto out; }
    len = c;
    buf[len] = '\0';
    ret = n;
out:
    mutex_unlock(&lock);
    return ret;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open, .release = my_release,
    .read = my_read, .write = my_write,
};

static int __init my_init(void)
{
    int ret = register_chrdev(MAJ, NAME, &fops);
    if (ret < 0) return ret;
    cls = class_create(THIS_MODULE, NAME "_class");
    if (IS_ERR(cls)) { unregister_chrdev(MAJ, NAME); return PTR_ERR(cls); }
    device_create(cls, NULL, MKDEV(MAJ, 0), NULL, NAME);
    pr_info(NAME ": loaded\n");
    return 0;
}

static void __exit my_exit(void)
{
    device_destroy(cls, MKDEV(MAJ, 0));
    class_destroy(cls);
    unregister_chrdev(MAJ, NAME);
    pr_info(NAME ": unloaded\n");
}

module_init(my_init);
module_exit(my_exit);
```

Matching Makefile (recipe lines must start with a **TAB**):

```make
KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15.221
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-
obj-m += mydev.o

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
```

`make -C $(KDIR)` jumps into the kernel source and borrows its build system; `M=$(CURDIR)`
tells it to come back and build the module here. That is why `KDIR` must be **exactly** the
source tree that produced `zImage` – otherwise the vermagic differs and `insmod` reports
`Invalid module format`.

---

**Next:** [05 – Kernel API](05-kernel-api.md)
