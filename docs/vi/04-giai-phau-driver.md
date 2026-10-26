# 04 – Giải phẫu một character driver

[English](../04-driver-anatomy.md) | **Tiếng Việt** · [← Về trang chủ](../../README.vi.md)

Dựa trên driver thật trong repo: [`LAB2/embedded_lab2/driver/lab2_driver.c`](../../LAB2/embedded_lab2/driver/lab2_driver.c).

---

## 1. Ba tầng của một driver

```text
┌──────────────────────────────────────────────────────────────────┐
│ TẦNG 1 – ĐĂNG KÝ (init / exit)          gần như không đổi giữa các đề │
│   register_chrdev → class_create → device_create → proc/sysfs     │
├──────────────────────────────────────────────────────────────────┤
│ TẦNG 2 – THAO TÁC (file_operations)     ĐỀ THI THAY ĐỔI Ở ĐÂY       │
│   open / read / write / release / unlocked_ioctl                  │
├──────────────────────────────────────────────────────────────────┤
│ TẦNG 3 – XUẤT THÔNG TIN                 tuỳ đề                     │
│   procfs (/proc/…), sysfs (/sys/class/…), module_param            │
└──────────────────────────────────────────────────────────────────┘
```

Liên hệ ESP32: tầng 1 ≈ `setup()` + gắn ngắt, tầng 2 ≈ các hàm callback, tầng 3 ≈ in trạng
thái ra Serial.

---

## 2. Bản đồ file theo thứ tự từ trên xuống

```text
 ⓪ DỮ LIỆU         #include, MODULE_*, #define, biến static, DEFINE_MUTEX
 Ⓐ THAO TÁC        lab2_open, lab2_release, lab2_read, lab2_write, lab2_fops
 Ⓑ XUẤT THÔNG TIN  lab2_proc_show + proc_ops   │  *_show + DEVICE_ATTR_RO + attr_group
 ⓘ ĐĂNG KÝ         lab2_init, lab2_exit, module_init/module_exit
```

> C đọc từ trên xuống: hàm phải được định nghĩa **trước** chỗ dùng nó. Vì vậy `lab2_fops`
> nằm sau 4 hàm Ⓐ, và `lab2_init` nằm cuối cùng.

Xem nhanh bản đồ của một file bất kỳ:

```bash
grep -n '/\* ===\|^static.*(\|module_init\|module_exit\|DEVICE_ATTR' lab2_driver.c
```

---

## 3. Vùng ⓪ – dữ liệu

```c
MODULE_LICENSE("GPL");                 // thiếu → kernel "tainted", một số API bị khoá
#define DEVICE_NAME  "lab2"            // tên trong /proc/devices và /dev/lab2
#define CLASS_NAME   "lab2_class"      // /sys/class/lab2_class/
#define BUFFER_SIZE  1024
#define DRIVER_MAJOR 240

static char   device_buffer[BUFFER_SIZE];   // "bộ nhớ" của thiết bị
static int    buffer_len = 0;
static int    open_count = 0;
static DEFINE_MUTEX(lab2_mutex);            // khoá bảo vệ 3 biến trên
```

- `static` ở mức file → biến chỉ thấy trong file này, sống từ `insmod` tới `rmmod`.
- Mọi biến mà **nhiều tiến trình cùng đọc/ghi** đều phải được bảo vệ bằng mutex.

---

## 4. Vùng Ⓐ – từ syscall tới hàm của mình

```text
 Userspace:  echo hello > /dev/lab2
     │  open("/dev/lab2")              ── syscall
     ▼
 VFS: inode là char device, major 240 ──► tra bảng đăng ký bởi register_chrdev
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

**Major number là chìa khoá để VFS biết phải gọi driver nào. `file_operations` là bảng con
trỏ hàm mà driver giao cho kernel.**

### 4.1. `read` – 4 bước

```c
static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    if (*offset >= buffer_len)                 // ① hết dữ liệu → trả 0 = EOF
        return 0;
    if (len > buffer_len - *offset)            // ② cắt cho vừa phần còn lại
        len = buffer_len - *offset;
    if (copy_to_user(buf, device_buffer + *offset, len))   // ③ chép ra user
        return -EFAULT;
    *offset += len;                            // ④ dịch con trỏ cho lần gọi sau
    return len;
}
```

Vì sao cần ④: `cat` gọi `read()` lặp lại tới khi nhận **0**. Không cập nhật `*offset` thì
lần nào cũng trả dữ liệu từ đầu → `cat` in mãi không dừng.

```text
cat /dev/lab2   (buffer = "hello\n", 6 byte)
  read(fd, buf, 4096)  offset=0  → trả 6, offset=6   → in "hello"
  read(fd, buf, 4096)  offset=6  → trả 0             → cat dừng
```

> ⚠️ Bản LAB-02 gốc **không khoá mutex trong `read`**. Bản ở ASSIGNMENT-01
> ([`asgn1_driver.c`](../../ASSIGNMENT1/task1_ioctl/asgn1_driver.c)) đã sửa theo mẫu `goto out`.

### 4.2. `write`

```c
mutex_lock(&lab2_mutex);
if (copy_from_user(device_buffer, buf, len)) { mutex_unlock(&lab2_mutex); return -EFAULT; }
buffer_len = len;
device_buffer[buffer_len] = '\0';
mutex_unlock(&lab2_mutex);
return len;
```

`write` phải trả về **số byte đã nhận**. Trả về ít hơn `len` → userspace hiểu là "ghi được
một phần" và **gọi lại** với phần còn lại (xem bài học ở ASSIGNMENT-01 CP4).

### 4.3. Mẫu một lối ra – dùng khi hàm có nhiều nhánh lỗi

```c
ssize_t ret;
mutex_lock(&m);
if (điều_kiện_lỗi_1) { ret = 0;       goto out; }
if (điều_kiện_lỗi_2) { ret = -EFAULT; goto out; }
/* xử lý bình thường */
ret = len;
out:
mutex_unlock(&m);       // chỉ viết MỘT lần → không thể quên
return ret;
```

Quy tắc kiểm tra: **giữa `mutex_lock` và `out:` không có chữ `return` nào.**

---

## 5. Vùng Ⓑ – xuất thông tin

### 5.1. procfs: `/proc/lab2_info`

```text
cat /proc/lab2_info
   → proc_open  = lab2_proc_open → single_open(file, lab2_proc_show, NULL)
   → proc_read  = seq_read       → gọi lab2_proc_show() để sinh nội dung
   → proc_release = single_release
```

Chỉ cần viết `lab2_proc_show` với các `seq_printf`; `seq_file` lo việc chia buffer, offset, EOF.

### 5.2. sysfs: `/sys/class/lab2_class/lab2/*`

```c
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", buffer_len);
}
static DEVICE_ATTR_RO(buffer_len);        // tạo dev_attr_buffer_len, quyền 0444
```

Quy ước tên: `DEVICE_ATTR_RO(xxx)` **bắt buộc** có hàm tên đúng `xxx_show`;
`_RW` cần thêm `xxx_store`; `_WO` chỉ cần `xxx_store`.

Gom lại và đăng ký một lần:

```c
static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr, &dev_attr_open_count.attr, &dev_attr_last_data.attr, NULL,
};
static struct attribute_group lab2_attr_group = { .attrs = lab2_attrs };
/* trong init: */ sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
```

| | procfs | sysfs |
|---|---|---|
| Dạng | Một file text nhiều dòng | Mỗi giá trị một file |
| Đối tượng đọc | Con người | Chương trình, script |
| Ghi được? | Có thể, nhưng ít dùng | Có (`_RW`, `_WO`) – cách chuẩn để cấu hình driver |

---

## 6. Vùng ⓘ – đăng ký và huỷ

```text
 lab2_init                                lab2_exit (THỨ TỰ NGƯỢC LẠI)
 ① register_chrdev(240, "lab2", &fops)    ⑤ proc_remove
 ② class_create("lab2_class")             ④ sysfs_remove_group
 ③ device_create(…, MKDEV(240,0), "lab2") ③ device_destroy
 ④ sysfs_create_group                     ② class_destroy
 ⑤ proc_create("lab2_info")               ① unregister_chrdev
```

| Lệnh | Kết quả nhìn thấy |
|---|---|
| `register_chrdev` | Dòng `240 lab2` trong `/proc/devices` |
| `class_create` | Thư mục `/sys/class/lab2_class/` |
| `device_create` | `/sys/class/lab2_class/lab2/` **và devtmpfs tự tạo `/dev/lab2`** |
| `proc_create` | `/proc/lab2_info` |

**Xử lý lỗi trong `init`:** bước nào thất bại thì huỷ những bước **đã thành công trước nó**
rồi `return` mã lỗi – nếu không, lần `insmod` sau sẽ báo `Device or resource busy` vì major
240 vẫn bị chiếm.

`IS_ERR` / `PTR_ERR`: hàm trả con trỏ báo lỗi bằng cách mã hoá mã lỗi âm vào chính con trỏ.

```c
cls = class_create(THIS_MODULE, CLASS_NAME);
if (IS_ERR(cls)) return PTR_ERR(cls);
```

> `class_create(THIS_MODULE, name)` là chữ ký của kernel 5.15. Từ kernel 6.4 hàm chỉ còn
> một tham số `class_create(name)`.

---

## 7. Khung tối thiểu – phải gõ lại được không cần nhìn

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

Makefile đi kèm (dòng lệnh phải bắt đầu bằng **TAB**):

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

`make -C $(KDIR)` nhảy vào source kernel và mượn hệ thống build của kernel; `M=$(CURDIR)`
báo nó quay lại build module trong thư mục hiện tại. Vì vậy `KDIR` phải là **đúng** source
đã build ra `zImage` – nếu không, vermagic lệch và `insmod` báo `Invalid module format`.

---

**Đọc tiếp:** [05 – Kernel API](05-kernel-api.md)
