# 05 – Bảng tra Kernel API (kernel 5.15)

[English](../05-kernel-api.md) | **Tiếng Việt** · [← Về trang chủ](../../README.vi.md)

Tra theo **tình huống**. Mỗi mục: include cần thêm → chữ ký → ví dụ ngắn.

> Kiểm chứng chữ ký với source thật trên máy (không cần internet):
> ```bash
> K=~/embedded_lab1/kernel/linux-5.15.221
> grep -rn 'int misc_register' $K/include/
> grep -rn 'define DEVICE_ATTR_RW' $K/include/
> ```

---

## Mục lục

1. [Chép dữ liệu user ↔ kernel](#1-chép-dữ-liệu-user--kernel)
2. [Khoá và đồng bộ](#2-khoá-và-đồng-bộ)
3. [Đăng ký character device](#3-đăng-ký-character-device)
4. [Misc device](#4-misc-device)
5. [file_operations – chữ ký các hàm](#5-file_operations--chữ-ký-các-hàm)
6. [procfs](#6-procfs)
7. [sysfs](#7-sysfs)
8. [module_param](#8-module_param)
9. [ioctl](#9-ioctl)
10. [Wait queue – chặn cho tới khi có dữ liệu](#10-wait-queue--chặn-cho-tới-khi-có-dữ-liệu)
11. [Chuỗi, số, ký tự](#11-chuỗi-số-ký-tự)
12. [Thời gian, ngẫu nhiên, tiến trình hiện tại](#12-thời-gian-ngẫu-nhiên-tiến-trình-hiện-tại)
13. [Cấp phát bộ nhớ](#13-cấp-phát-bộ-nhớ)
14. [Log](#14-log)
15. [Mã lỗi hay dùng](#15-mã-lỗi-hay-dùng)

---

## 1. Chép dữ liệu user ↔ kernel

```c
#include <linux/uaccess.h>

unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
int get_user(x, ptr);          /* 1 biến đơn (int, char…) từ user */
int put_user(x, ptr);          /* 1 biến đơn ra user */
```

- Trả về **số byte KHÔNG chép được** → khác 0 nghĩa là lỗi → `return -EFAULT`.
- Kernel → kernel (cả hai đều là bộ nhớ kernel): dùng `memcpy` bình thường.

```c
if (copy_from_user(kbuf, ubuf, n))
    return -EFAULT;
```

---

## 2. Khoá và đồng bộ

### Mutex (được ngủ – dùng trong read/write/ioctl)

```c
#include <linux/mutex.h>
static DEFINE_MUTEX(my_lock);

mutex_lock(&my_lock);
mutex_unlock(&my_lock);
int mutex_trylock(&my_lock);              /* 1 = lấy được, 0 = đang bận (không chờ) */
int mutex_lock_interruptible(&my_lock);   /* 0 = OK, -EINTR nếu bị signal (Ctrl+C) */
```

```c
if (mutex_lock_interruptible(&my_lock))
    return -ERESTARTSYS;
```

### Atomic (biến đếm đơn, không cần mutex)

```c
#include <linux/atomic.h>
static atomic_t counter = ATOMIC_INIT(0);

atomic_inc(&counter);           atomic_dec(&counter);
atomic_set(&counter, 5);        int v = atomic_read(&counter);
int old = atomic_cmpxchg(&counter, 0, 1);   /* nếu ==0 thì đặt 1; trả giá trị CŨ */
int r   = atomic_inc_return(&counter);
```

Single-open bằng atomic:

```c
static atomic_t in_use = ATOMIC_INIT(0);
/* open */    if (atomic_cmpxchg(&in_use, 0, 1) != 0) return -EBUSY;
/* release */ atomic_set(&in_use, 0);
```

| Dùng | Khi nào |
|---|---|
| `atomic_t` | Một biến đếm / cờ đơn lẻ |
| `mutex` | Nhiều biến phải đổi cùng nhau (buffer + len), hoặc thao tác có thể ngủ |
| `spinlock` | Trong ngắt / ngữ cảnh không được ngủ (chưa dùng trong môn này) |

---

## 3. Đăng ký character device

```c
#include <linux/fs.h>
#include <linux/device.h>

int  register_chrdev(unsigned int major, const char *name, const struct file_operations *fops);
void unregister_chrdev(unsigned int major, const char *name);

struct class  *class_create(struct module *owner, const char *name);   /* 5.15: 2 tham số */
void           class_destroy(struct class *cls);

struct device *device_create(struct class *cls, struct device *parent,
                             dev_t devt, void *drvdata, const char *fmt, ...);
void           device_destroy(struct class *cls, dev_t devt);

dev_t MKDEV(major, minor);   MAJOR(dev);   MINOR(dev);
unsigned int iminor(const struct inode *inode);    /* minor của file đang mở */
```

**Major động:**

```c
static int major;
major = register_chrdev(0, NAME, &fops);   /* 0 → kernel tự chọn, trả về số được cấp */
if (major < 0) return major;
```

---

## 4. Misc device

Major luôn là **10**, kernel tự cấp minor, tự tạo `/dev/<name>`. Không cần
`register_chrdev` + `class_create` + `device_create`.

```c
#include <linux/miscdevice.h>

static struct miscdevice my_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "counter",            /* → /dev/counter */
    .fops  = &my_fops,
    .mode  = 0666,                 /* quyền của /dev/counter */
};

/* init */ int ret = misc_register(&my_misc);   if (ret) return ret;
/* exit */ misc_deregister(&my_misc);
```

Kiểm tra: `cat /proc/misc` có dòng `<minor> counter`; `ls -l /dev/counter` → `crw-… 10, <minor>`.

---

## 5. file_operations – chữ ký các hàm

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
    .llseek         = default_llseek,      /* nếu cần lseek / dd seek= */
};
```

`file->private_data`: con trỏ riêng cho **từng lần mở** – gán trong `open`, dùng trong
`read/write` (ví dụ: driver nhiều minor, mỗi minor một buffer).

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

> Kernel 5.6+ dùng `struct proc_ops`. Tài liệu cũ dùng `struct file_operations` cho procfs –
> không build được trên 5.15.

---

## 7. sysfs

```c
#include <linux/device.h>
#include <linux/sysfs.h>

/* Chữ ký cố định */
static ssize_t xxx_show (struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t xxx_store(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count);

static DEVICE_ATTR_RO(xxx);   /* 0444 – cần xxx_show */
static DEVICE_ATTR_RW(xxx);   /* 0644 – cần xxx_show + xxx_store */
static DEVICE_ATTR_WO(xxx);   /* 0200 – cần xxx_store */
/* → tạo biến dev_attr_xxx */
```

Ví dụ thuộc tính ghi được:

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
    return count;            /* PHẢI trả về count, nếu không echo sẽ ghi lặp */
}
static DEVICE_ATTR_RW(mode);
```

Đăng ký:

```c
/* Cách 1 – thêm vào mảng attrs có sẵn rồi sysfs_create_group (như lab2_driver.c) */
&dev_attr_mode.attr,

/* Cách 2 – từng file riêng */
device_create_file(my_device, &dev_attr_mode);      /* init */
device_remove_file(my_device, &dev_attr_mode);      /* exit, TRƯỚC device_destroy */
```

---

## 8. module_param

```c
#include <linux/moduleparam.h>

static int max_len = 1024;
module_param(max_len, int, 0444);           /* kiểu: int, uint, bool, charp, ulong… */
MODULE_PARM_DESC(max_len, "Max bytes stored per write");
```

```sh
insmod my.ko max_len=5
cat /sys/module/my/parameters/max_len
```

Quyền `0444` → chỉ đọc; `0644` → root sửa được khi module đang chạy (khi đó phải nghĩ về đồng bộ).
Kiểm tra giá trị hợp lệ trong `init`, sai thì `return -EINVAL`.

---

## 9. ioctl

**Header chung** cho kernel và userspace:

```c
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define MY_MAGIC   0xA5
#define MY_RESET   _IO (MY_MAGIC, 0)                 /* không có dữ liệu */
#define MY_GET     _IOR(MY_MAGIC, 1, struct my_stats) /* kernel → user */
#define MY_SET     _IOW(MY_MAGIC, 2, int)             /* user → kernel */
```

Cấu trúc 32 bit của mã lệnh:

```text
 31 30 │ 29 ……… 16 │ 15 …… 8 │ 7 …… 0
  DIR  │   SIZE    │  TYPE   │   NR
 0=none 1=W 2=R    sizeof(t)   magic    số thứ tự
```

```c
_IOC_DIR(cmd)  _IOC_SIZE(cmd)  _IOC_TYPE(cmd)  _IOC_NR(cmd)
```

**Phía kernel:**

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

**Phía userspace** (shell không gọi được ioctl → phải viết chương trình C):

```c
int fd = open("/dev/mydev", O_RDWR);
int v = 1;
if (ioctl(fd, MY_SET, &v) < 0) perror("ioctl");
```

Build: `arm-linux-gnueabihf-gcc -static -Wall -o app app.c` (**bắt buộc `-static`**).

Ví dụ đầy đủ: [`ASSIGNMENT1/task1_ioctl/`](../../ASSIGNMENT1/task1_ioctl/).

---

## 10. Wait queue – chặn cho tới khi có dữ liệu

```c
#include <linux/wait.h>
static DECLARE_WAIT_QUEUE_HEAD(my_wq);

/* read: ngủ tới khi điều kiện đúng; trả -ERESTARTSYS nếu bị Ctrl+C */
if (wait_event_interruptible(my_wq, buffer_len > 0))
    return -ERESTARTSYS;

/* write: sau khi có dữ liệu mới */
wake_up_interruptible(&my_wq);
```

> Không gọi `wait_event_*` khi đang giữ mutex – tiến trình ghi sẽ không lấy được khoá để
> đánh thức. Thứ tự đúng: chờ → lấy khoá → kiểm tra lại điều kiện.

---

## 11. Chuỗi, số, ký tự

```c
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/ctype.h>

size_t  strlen(const char *s);
ssize_t strscpy(char *dst, const char *src, size_t size);   /* luôn có '\0', an toàn */
int     snprintf(char *buf, size_t size, const char *fmt, ...);
int     sprintf(char *buf, const char *fmt, ...);           /* chỉ dùng trong sysfs show */

int kstrtoint (const char *s, unsigned int base, int *res);        /* 0 = OK, <0 = lỗi */
int kstrtouint(const char *s, unsigned int base, unsigned int *res);
int kstrtol   (const char *s, unsigned int base, long *res);

min(a, b)   max(a, b)   min_t(size_t, a, b)   clamp(v, lo, hi)

toupper(c) tolower(c) isalpha(c) isdigit(c) isalnum(c) isspace(c) isprint(c)
```

`kstrtoint` chấp nhận chuỗi có `\n` ở cuối (do `echo` thêm vào) – không cần tự cắt.

> ❗ **Không dùng `float`/`double` trong kernel.** Dùng số nguyên với đơn vị nhỏ hơn
> (ví dụ 0.1°C): `seq_printf(m, "%d.%d", t / 10, t % 10);`

---

## 12. Thời gian, ngẫu nhiên, tiến trình hiện tại

```c
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/sched.h>

u64    ktime_get_ns(void);                    /* ns từ lúc boot (monotonic) */
s64    ktime_to_ms(ktime_t kt);               /* ktime_to_ms(ktime_get()) → ms */
time64_t ktime_get_real_seconds(void);        /* giây theo giờ thực */
unsigned long jiffies;                        /* số tick từ lúc boot (HZ=100 → 10 ms/tick) */
unsigned int jiffies_to_msecs(unsigned long j);

u32 get_random_u32(void);                     /* số ngẫu nhiên 32 bit */
/* khoảng [lo, hi]:  lo + get_random_u32() % (hi - lo + 1) */

current->pid      current->comm               /* PID và tên tiến trình đang gọi */
```

---

## 13. Cấp phát bộ nhớ

```c
#include <linux/slab.h>

void *kmalloc(size_t size, gfp_t flags);      /* GFP_KERNEL trong ngữ cảnh được ngủ */
void *kzalloc(size_t size, gfp_t flags);      /* như kmalloc nhưng xoá về 0 */
void  kfree(const void *p);
```

Mọi `kmalloc` trong `init` phải có `kfree` tương ứng trong `exit` (và ở mọi nhánh lỗi của `init`).

---

## 14. Log

```c
pr_info("mydrv: value=%d\n", v);
pr_warn(...);   pr_err(...);   pr_debug(...);   /* pr_debug cần bật dynamic debug */
```

| Định dạng | Kiểu |
|---|---|
| `%d` / `%u` | `int` / `unsigned int` |
| `%ld` / `%lld` | `long` / `long long` (`s64`) |
| `%zu` / `%zd` | `size_t` / `ssize_t` |
| `%lu` | `unsigned long` (`jiffies`) |
| `%llu` | `u64` |
| `%#x` | hex có `0x` |
| `%s` / `%c` | chuỗi / ký tự |

Xem log: `dmesg | tail -20`, `dmesg | grep mydrv`.

---

## 15. Mã lỗi hay dùng

| Mã | Ý nghĩa | Khi nào trả |
|---|---|---|
| `-EFAULT` | Bad address | `copy_*_user` thất bại |
| `-EINVAL` | Invalid argument | Tham số sai (mode ngoài phạm vi, số âm…) |
| `-ENOTTY` | Inappropriate ioctl for device | Lệnh ioctl không hỗ trợ |
| `-EBUSY` | Device or resource busy | Thiết bị đang được mở / đang dùng |
| `-ENOMEM` | Out of memory | `kmalloc` trả `NULL` |
| `-ENOSPC` | No space left on device | Buffer/hàng đợi đầy |
| `-EAGAIN` | Try again | Chưa sẵn sàng (non-blocking, đọc quá nhanh) |
| `-ERESTARTSYS` | (bị signal ngắt) | `wait_event_interruptible` / `mutex_lock_interruptible` trả lỗi |
| `-ENODEV` | No such device | Phần cứng không có |

Userspace thấy lỗi qua `errno`; `perror("ioctl")` in thông điệp ở cột "Ý nghĩa".

---

**Đọc tiếp:** [06 – Lỗi thường gặp](06-loi-thuong-gap.md)
