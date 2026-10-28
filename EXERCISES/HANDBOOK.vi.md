# Sổ tay bài tập driver LAB-02

[English](HANDBOOK.md) | **Tiếng Việt** · [← Danh sách bài](README.vi.md) · [← Về trang chủ](../README.vi.md)

20 bài luyện + 8 bài bổ sung dựa trên driver LAB-02. Mỗi bài ghi rõ: **đề, sửa ở vùng nào, header nào, hàm nào, code chính, lệnh test, lỗi hay gặp**.

> Kernel 5.15.221 · QEMU vexpress-a9 · `arm-linux-gnueabihf-gcc` 15.2
> Bài đã luyện trong repo có ghi tên thư mục lời giải. Các bài còn lại là code mẫu theo đúng driver gốc – nên chạy thử trước khi thi.

## Mục lục

1. [Quy trình làm một bài](#1-quy-trình-làm-một-bài)
2. [Bản đồ driver và "đề nói gì → sửa ở đâu"](#2-bản-đồ-driver-và-đề-nói-gì--sửa-ở-đâu)
3. [Header nào – hàm nào](#3-header-nào--hàm-nào)
4. [Bốn mẫu code dùng lại](#4-bốn-mẫu-code-dùng-lại)
5. [Hai mươi bài luyện](#5-hai-mươi-bài-luyện)
6. [Bài bổ sung](#6-bài-bổ-sung)
7. [Đề yêu cầu đặt tên riêng](#7-đề-yêu-cầu-đặt-tên-riêng)
8. [Mã lỗi trả về](#8-mã-lỗi-trả-về)
9. [Checklist trước khi gọi giảng viên](#9-checklist-trước-khi-gọi-giảng-viên)

## 1. Quy trình làm một bài

| Bước | Việc | Lệnh |
|---|---|---|
| ① | Chép driver gốc | `cp -r $REPO/EXERCISES/base ~/exam/bai1` |
| ② | Sửa code | `code ~/exam/bai1/lab2_driver.c` |
| ③ | Build sạch | `make 2>&1 \| grep -E 'error\|warning'` (phải trống) |
| ④ | Chép + đóng gói + boot | `script -q -c "$HOME/embedded_lab2/dev.sh" log.txt` |
| ⑤ | Test trong QEMU (dấu nhắc `/ # `) | Lệnh test của từng bài · thoát: Ctrl+A rồi X |

- Giữ tên `lab2_driver` nếu đề không yêu cầu đổi: `rcS` tự nạp bản mới, `test_driver.sh` vẫn dùng được.
- Không gõ `qemu-system-arm` trực tiếp sau khi sửa code – luôn chạy `dev.sh`.

```bash
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
mkdir -p ~/exam && cp -r "$REPO/EXERCISES/base" ~/exam/bai1
cd ~/exam/bai1
make 2>&1 | grep -E 'error|warning'
script -q -c "$HOME/embedded_lab2/dev.sh" log_bai1.txt
```

### Đọc lỗi make

| Thông báo | Nghĩa |
|---|---|
| `file.c:DÒNG:CỘT: error:` đầu tiên | Chỗ cần sửa – các dòng sau thường là hệ quả |
| Hàng loạt `invalid storage class for function` | Thiếu `}` ở hàm đứng trước lỗi đầu tiên |
| `'x' undeclared` / `implicit declaration` | Chưa khai báo biến hoặc thiếu `#include` (xem Phần 3) |
| `control reaches end of non-void function` | Thiếu `return` cuối hàm |
| `format '%s' expects … char *` | Sai kiểu trong `pr_info`/`seq_printf` – có thể làm sập kernel |

## 2. Bản đồ driver và "đề nói gì → sửa ở đâu"

```text
 ⓪ DỮ LIỆU         #include, MODULE_*, #define, biến static, DEFINE_MUTEX
 Ⓐ THAO TÁC        lab2_open, lab2_release, lab2_read, lab2_write, lab2_fops
 Ⓑ XUẤT THÔNG TIN  lab2_proc_show + proc_ops │ *_show/*_store + DEVICE_ATTR_* + lab2_attrs[]
 ⓘ ĐĂNG KÝ         lab2_init (register_chrdev → class_create → device_create → sysfs → proc)
                   lab2_exit (làm NGƯỢC LẠI)
```

| Đề yêu cầu | Sửa ở | Bài mẫu |
|---|---|---|
| Biến đổi dữ liệu khi ghi (hoa, thường, lọc, mã hoá) | Ⓐ `write`, sau `copy_from_user` | 1, 5, B1, B2 |
| Biến đổi chỉ khi đọc ra | Ⓐ `read`, buffer tạm trước `copy_to_user` | 2 |
| Đếm / thống kê | ⓪ biến + Ⓐ tăng trong mutex + Ⓑ in ra | 6, 7, 8, B4 |
| Ghi lại người dùng / thời gian | Ⓐ `write` dùng `current`, `ktime_*` | 9, 10, B7 |
| Cấu hình lúc đang chạy | Ⓑ sysfs `_RW` / `_WO` | 11, 12, B3 |
| Cấu hình lúc insmod | ⓪ `module_param` + ⓘ kiểm tra đầu init | 13, B5 |
| Đổi cách đăng ký | ⓘ init/exit | 14, B6 |
| Giới hạn truy cập / đồng bộ | Ⓐ open/release, khoá, wait queue | 15, 16, 18 |
| Cấu trúc dữ liệu mới (hàng đợi) | ⓪ + Ⓐ read/write | 17 |
| Lệnh điều khiển | Ⓐ `unlocked_ioctl` + header + app | 19 |
| Nhiều thiết bị | ⓘ + Ⓐ dùng `private_data` | 20 |

## 3. Header nào – hàm nào

Gặp `implicit declaration of function 'X'` → tìm X trong bảng, thêm đúng `#include`. Kernel **không** dùng `<stdio.h>`, `<ctype.h>`, `<string.h>` của C thường.

| Header | Hàm / macro chính | Dùng khi |
|---|---|---|
| `<linux/module.h>` | MODULE_LICENSE, MODULE_AUTHOR, module_init, module_exit, THIS_MODULE | Mọi module – luôn có |
| `<linux/fs.h>` | register_chrdev, unregister_chrdev, struct file_operations, iminor, default_llseek | Đăng ký char device, bảng fops |
| `<linux/device.h>` | class_create, device_create, device_destroy, DEVICE_ATTR_RO/RW/WO, sysfs_create_group, ATTRIBUTE_GROUPS | /dev tự tạo, thuộc tính sysfs |
| `<linux/uaccess.h>` | copy_to_user, copy_from_user, get_user, put_user | Mọi dữ liệu user ↔ kernel |
| `<linux/mutex.h>` | DEFINE_MUTEX, mutex_init, mutex_lock, mutex_unlock, mutex_trylock, mutex_lock_interruptible | Khoá nhiều biến cùng lúc |
| `<linux/atomic.h>` | atomic_t, ATOMIC_INIT, atomic_read/set/inc/dec, atomic_cmpxchg, atomic_fetch_inc | Một biến đếm / cờ đơn |
| `<linux/proc_fs.h> + <linux/seq_file.h>` | proc_create, proc_remove, struct proc_ops, single_open, single_release, seq_read, seq_lseek, seq_printf | File /proc/xxx |
| `<linux/moduleparam.h>` | module_param, MODULE_PARM_DESC | Tham số lúc insmod |
| `<linux/kernel.h>` | kstrtoint, kstrtouint, min, max, min_t, clamp, sprintf, snprintf, scnprintf, pr_info/pr_warn/pr_err | Chuỗi → số, so sánh, log |
| `<linux/string.h>` | strlen, strscpy, memset, memcpy, strcmp | Xử lý chuỗi trong kernel |
| `<linux/ctype.h>` | toupper, tolower, isalpha, isdigit, isalnum, isspace, isupper, islower | Biến đổi / phân loại ký tự |
| `<linux/sched.h>` | current->pid, current->comm, TASK_COMM_LEN | Biết tiến trình nào đang gọi |
| `<linux/timekeeping.h>, <linux/ktime.h>` | ktime_get_real_seconds, ktime_get_ns, ktime_get, ktime_to_ms | Thời gian |
| `<linux/jiffies.h>` | jiffies, jiffies_to_msecs, HZ | Đếm tick, uptime |
| `<linux/wait.h>` | DECLARE_WAIT_QUEUE_HEAD, wait_event_interruptible, wake_up_interruptible | read chặn chờ dữ liệu |
| `<linux/slab.h>` | kmalloc, kzalloc, kfree, GFP_KERNEL | Cấp phát bộ nhớ động |
| `<linux/miscdevice.h>` | struct miscdevice, MISC_DYNAMIC_MINOR, misc_register, misc_deregister | Misc device (major 10) |
| `<linux/ioctl.h> (kernel) / <sys/ioctl.h> (user)` | _IO, _IOR, _IOW, _IOC_TYPE, _IOC_NR, _IOC_SIZE | Lệnh ioctl |
| `<linux/random.h>` | get_random_u32 | Số ngẫu nhiên |

Tra chữ ký thật ngay trên máy:

```bash
K=~/embedded_lab1/kernel/linux-5.15.221
grep -rn 'int misc_register' $K/include/
grep -rn 'define DEVICE_ATTR_RW' $K/include/
```

## 4. Bốn mẫu code dùng lại

### 4.1. Một lối ra cho hàm có mutex

```c
ssize_t ret;
mutex_lock(&lab2_mutex);
if (lỗi_1) { ret = 0;       goto out; }
if (lỗi_2) { ret = -EFAULT; goto out; }
/* xử lý bình thường */
ret = len;
out:
mutex_unlock(&lab2_mutex);
return ret;
```

Kiểm tra: giữa `mutex_lock` và `out:` không có chữ `return`; mọi nhánh đều gán `ret`.

### 4.2. write cắt bớt nhưng vẫn trả len

```c
size_t copy_len = min_t(size_t, len, BUFFER_SIZE - 1);   /* số byte SẼ LƯU */
/* copy_from_user(device_buffer, buf, copy_len) … */
buffer_len = copy_len;
device_buffer[buffer_len] = '\0';                       /* theo độ dài MỚI */
return len;                                             /* số byte user GỬI */
```

Trả về ít hơn `len` → shell gọi `write` lần nữa với phần còn lại và ghi đè dữ liệu.

### 4.3. Thuộc tính sysfs

```c
static ssize_t xxx_show(struct device *d, struct device_attribute *a, char *buf)
{ return sprintf(buf, "%d\n", xxx); }

static ssize_t xxx_store(struct device *d, struct device_attribute *a,
                         const char *buf, size_t count)
{
    int v;
    if (kstrtoint(buf, 10, &v) || /* ngoài khoảng */) return -EINVAL;
    mutex_lock(&lab2_mutex); xxx = v; mutex_unlock(&lab2_mutex);
    return count;
}
static DEVICE_ATTR_RW(xxx);      /* RO: chỉ _show (0444) · WO: chỉ _store (0200) */
/* lab2_attrs[]: &dev_attr_xxx.attr,   – đặt trước NULL */
```

### 4.4. Thêm dòng vào /proc

```c
seq_printf(m, "Tên           : %d\n", bien_int);
seq_printf(m, "Tên           : %lu\n", bien_unsigned_long);
seq_printf(m, "Tên           : %s\n", co ? "yes" : "no");
```

| Kiểu | Định dạng |
|---|---|
| `int` | `%d` |
| `unsigned int` | `%u` |
| `long` / `unsigned long` | `%ld` / `%lu` |
| `size_t` / `ssize_t` | `%zu` / `%zd` |
| `long long` (`time64_t`, `s64`) | `%lld` |
| `u64` | `%llu` |
| chuỗi / ký tự | `%s` / `%c` |

## 5. Hai mươi bài luyện

| # | Bài | Độ khó | Đã luyện |
|---|---|---|---|
| 1 | Lưu dữ liệu dạng CHỮ HOA | ★★☆ | ✅ `ex01_toupper` |
| 2 | Đọc ra chuỗi ĐẢO NGƯỢC | ★★☆ | – |
| 3 | Chế độ NỐI THÊM (append) | ★★☆ | – |
| 4 | Đọc một lần rồi xoá (consume-on-read) | ★★☆ | – |
| 5 | Lọc ký tự | ★★☆ | – |
| 6 | Đếm số lần đọc / ghi | ★★☆ | ✅ `ex02_counters` |
| 7 | Tổng số byte đã ghi (sysfs RO) | ★★☆ | – |
| 8 | Thống kê nội dung trong /proc | ★★☆ | – |
| 9 | Ai ghi gần nhất (PID + tên tiến trình) | ★★☆ | – |
| 10 | Thời điểm ghi cuối | ★★☆ | – |
| 11 | Thuộc tính sysfs GHI ĐƯỢC: mode | ★★★ | ✅ `ex03_sysfs_mode` |
| 12 | Nút XOÁ qua sysfs (chỉ ghi) | ★★☆ | – |
| 13 | Tham số khi nạp: max_len | ★★☆ | ✅ `ex04_module_param` |
| 14 | Major do kernel tự cấp | ★★☆ | – |
| 15 | Chỉ cho MỘT tiến trình mở | ★★★ | ✅ `ex05_single_open` |
| 16 | Khoá NGẮT ĐƯỢC bằng Ctrl+C | ★★☆ | – |
| 17 | Hàng đợi tin nhắn FIFO (ring buffer) | ★★★ | – |
| 18 | read CHẶN chờ dữ liệu (wait queue) | ★★★ | – |
| 19 | Lệnh ioctl: CLEAR và GET_LEN | ★★★ | ✅ `ASSIGNMENT1/task1_ioctl` |
| 20 | Hai thiết bị /dev/lab2_0 và /dev/lab2_1 | ★★★ | – |

### Bài 1. Lưu dữ liệu dạng CHỮ HOA ★★☆

**Đề:** Mọi chuỗi ghi vào /dev/lab2 được LƯU dạng chữ hoa; số và ký tự đặc biệt giữ nguyên.

**Header:** `<linux/ctype.h>` · **Hàm chính:** `toupper(c)`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | #include <linux/ctype.h> |
| Ⓐ write | Sau copy_from_user thành công, trước mutex_unlock: vòng for toupper |

**Code chính**

```c
size_t i;
/* trong lab2_write, sau copy_from_user thành công, vẫn giữ mutex */
for (i = 0; i < len; i++)
    device_buffer[i] = toupper(device_buffer[i]);
```

**Test trong QEMU**

```sh
echo "Hello Lab2 123!" > /dev/lab2 ; cat /dev/lab2     # HELLO LAB2 123!
grep 'Last data' /proc/lab2_info                      # chữ hoa
```

**Lỗi hay gặp**

- i <= len → chạm 1 byte ngoài dữ liệu
- Đổi trên buf (con trỏ __user) → sai
- Đổi trong read → /proc vẫn chữ thường

> Lời giải đầy đủ: [`ex01_toupper`](ex01_toupper/)

### Bài 2. Đọc ra chuỗi ĐẢO NGƯỢC ★★☆

**Đề:** Buffer lưu nguyên bản, nhưng read trả về chuỗi đảo ngược; ký tự '\n' cuối vẫn ở cuối.

**Header:** `<linux/uaccess.h>`, `<linux/mutex.h>` · **Hàm chính:** `copy_to_user`, `mutex_lock/unlock`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | Buffer tạm static char rev_buf[BUFFER_SIZE] (không đặt 1 KB trên stack kernel) |
| Ⓐ read | Lấy mutex, tạo bản đảo vào rev_buf, copy_to_user từ rev_buf + *offset |

**Code chính**

```c
static char rev_buf[BUFFER_SIZE];

static ssize_t lab2_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret;
    int i, n, m;

    mutex_lock(&lab2_mutex);
    if (*off >= buffer_len) { ret = 0; goto out; }
    n = buffer_len;
    m = (n > 0 && device_buffer[n - 1] == '\n') ? n - 1 : n;   /* bỏ '\n' cuối */
    for (i = 0; i < m; i++)
        rev_buf[i] = device_buffer[m - 1 - i];
    if (m < n)
        rev_buf[m] = '\n';            /* trả '\n' về cuối */
    if (len > n - *off) len = n - *off;
    if (copy_to_user(buf, rev_buf + *off, len)) { ret = -EFAULT; goto out; }
    *off += len;
    ret = len;
out:
    mutex_unlock(&lab2_mutex);
    return ret;
}
```

**Test trong QEMU**

```sh
echo abc > /dev/lab2 ; cat /dev/lab2         # cba
cat /proc/lab2_info | grep 'Last data'      # [abc  (buffer vẫn nguyên bản)
```

**Lỗi hay gặp**

- Đảo luôn '\n' → dấu nhắc dính lên đầu dòng
- Tạo bản đảo mà không giữ mutex → đọc lẫn dữ liệu đang bị ghi

### Bài 3. Chế độ NỐI THÊM (append) ★★☆

**Đề:** write nối thêm vào cuối buffer thay vì ghi đè; đầy thì trả -ENOSPC.

**Header:** `<linux/uaccess.h>` · **Hàm chính:** `copy_from_user`

| Vùng | Việc cần làm |
|---|---|
| Ⓐ write | copy_from_user vào device_buffer + buffer_len; kiểm tra chỗ trống trước |

**Code chính**

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

**Test trong QEMU**

```sh
echo a > /dev/lab2 ; echo b > /dev/lab2 ; cat /dev/lab2      # a  rồi  b
head -c 2000 /dev/zero | tr '\0' x > /dev/lab2                  # No space left on device
```

**Lỗi hay gặp**

- Quên cộng buffer_len vào địa chỉ đích → vẫn ghi đè
- Kiểm tra chỗ trống sau khi copy → đã tràn

### Bài 4. Đọc một lần rồi xoá (consume-on-read) ★★☆

**Đề:** Sau khi đọc hết, buffer bị xoá: lần cat thứ hai không ra gì.

**Header:** `<linux/uaccess.h>` · **Hàm chính:** `copy_to_user`

| Vùng | Việc cần làm |
|---|---|
| Ⓐ read | Sau khi copy, nếu *offset đã tới cuối thì buffer_len = 0 |

**Code chính**

```c
/* trong lab2_read, sau copy_to_user thành công và *off += len */
if (*off >= buffer_len) {
    buffer_len = 0;
    device_buffer[0] = '\0';
}
```

**Test trong QEMU**

```sh
echo hi > /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2      # chỉ lần đầu ra "hi"
```

**Lỗi hay gặp**

- Xoá ngay đầu read → user dùng buffer nhỏ (dd bs=1) chỉ nhận 1 byte
- Không khoá mutex khi xoá

### Bài 5. Lọc ký tự ★★☆

**Đề:** Chỉ lưu chữ cái và chữ số (giữ '\n' cuối); bỏ dấu câu và khoảng trắng.

**Header:** `<linux/ctype.h>` · **Hàm chính:** `isalnum(c)`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | #include <linux/ctype.h> |
| Ⓐ write | Lọc tại chỗ: hai chỉ số i (đọc) và j (ghi) |

**Code chính**

```c
size_t i, j = 0;
/* sau copy_from_user thành công */
for (i = 0; i < len; i++)
    if (isalnum(device_buffer[i]) || device_buffer[i] == '\n')
        device_buffer[j++] = device_buffer[i];
buffer_len = j;
device_buffer[buffer_len] = '\0';
ret = len;               /* vẫn trả số byte user gửi */
```

**Test trong QEMU**

```sh
echo "a-b c!1" > /dev/lab2 ; cat /dev/lab2        # abc1
```

**Lỗi hay gặp**

- Trả về j thay vì len → shell ghi lặp
- Lọc tại chỗ chỉ đúng vì j <= i luôn đúng

### Bài 6. Đếm số lần đọc / ghi ★★☆

**Đề:** write_count tăng khi ghi thành công; read_count tăng khi read trả dữ liệu (EOF không tính); hiện trong /proc.

**Header:** `<linux/mutex.h>`, `<linux/seq_file.h>` · **Hàm chính:** `mutex_lock`, `seq_printf`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | static int write_count, read_count |
| Ⓐ write/read | Tăng trong mutex, read dùng mẫu goto out |
| Ⓑ proc | 2 dòng seq_printf |

**Code chính**

```c
/* lab2_read – nhánh có dữ liệu */
*off += len;
read_count++;
ret = len;            /* KHÔNG phải *off */

/* lab2_proc_show */
seq_printf(m, "Write count   : %d\n", write_count);
seq_printf(m, "Read count    : %d\n", read_count);
```

**Test trong QEMU**

```sh
echo hi > /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2
grep count /proc/lab2_info              # Write 1, Read 2
```

**Lỗi hay gặp**

- Có mutex_unlock mà không mutex_lock
- ret = *offset (chỉ đúng ở lần gọi đầu)
- Quên seq_printf → bộ đếm vô hình

> Lời giải đầy đủ: [`ex02_counters`](ex02_counters/)

### Bài 7. Tổng số byte đã ghi (sysfs RO) ★★☆

**Đề:** Thuộc tính /sys/class/lab2_class/lab2/total_bytes cộng dồn mọi byte đã lưu.

**Header:** `<linux/device.h>` · **Hàm chính:** `DEVICE_ATTR_RO`, `sprintf`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | static unsigned long total_bytes |
| Ⓐ write | total_bytes += số byte đã lưu |
| Ⓑ sysfs | total_bytes_show + DEVICE_ATTR_RO + thêm vào lab2_attrs[] |

**Code chính**

```c
static unsigned long total_bytes;

static ssize_t total_bytes_show(struct device *d, struct device_attribute *a, char *buf)
{
    return sprintf(buf, "%lu\n", total_bytes);
}
static DEVICE_ATTR_RO(total_bytes);
/* lab2_attrs[]:  &dev_attr_total_bytes.attr,   (trước NULL) */
```

**Test trong QEMU**

```sh
echo abc > /dev/lab2 ; echo de > /dev/lab2
cat /sys/class/lab2_class/lab2/total_bytes      # 7
```

**Lỗi hay gặp**

- %d cho unsigned long → warning định dạng (dùng %lu)
- Quên thêm vào mảng attrs → không có file

### Bài 8. Thống kê nội dung trong /proc ★★☆

**Đề:** /proc/lab2_info hiện số chữ cái, chữ số, khoảng trắng của dữ liệu hiện tại.

**Header:** `<linux/ctype.h>` · **Hàm chính:** `isalpha`, `isdigit`, `isspace`

| Vùng | Việc cần làm |
|---|---|
| Ⓑ proc | Chỉ sửa lab2_proc_show: duyệt device_buffer và đếm |

**Code chính**

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

**Test trong QEMU**

```sh
echo "ab 12 c" > /dev/lab2 ; grep Letters /proc/lab2_info    # 3 / 2 / 3  ('\n' là khoảng trắng)
```

**Lỗi hay gặp**

- Quên '\n' cũng là isspace
- Đếm mà không khoá → số lệch khi đang có người ghi

### Bài 9. Ai ghi gần nhất (PID + tên tiến trình) ★★☆

**Đề:** Lưu PID và tên tiến trình ghi gần nhất; hiện qua sysfs last_writer.

**Header:** `<linux/sched.h>`, `<linux/string.h>` · **Hàm chính:** `current`, `strscpy`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | #include <linux/sched.h>; static pid_t last_pid; static char last_comm[TASK_COMM_LEN] |
| Ⓐ write | Ghi current->pid, current->comm |
| Ⓑ sysfs | last_writer_show + DEVICE_ATTR_RO |

**Code chính**

```c
/* lab2_write, trong mutex */
last_pid = current->pid;
strscpy(last_comm, current->comm, sizeof(last_comm));

static ssize_t last_writer_show(struct device *d, struct device_attribute *a, char *buf)
{
    return sprintf(buf, "%d %s\n", last_pid, last_comm);
}
static DEVICE_ATTR_RO(last_writer);
```

**Test trong QEMU**

```sh
echo x > /dev/lab2 ; cat /sys/class/lab2_class/lab2/last_writer      # 123 sh
```

**Lỗi hay gặp**

- strcpy thay strscpy → không giới hạn độ dài
- current là tiến trình đang gọi – chỉ có nghĩa trong open/read/write/ioctl

### Bài 10. Thời điểm ghi cuối ★★☆

**Đề:** /proc/lab2_info hiện số giây kể từ lần ghi cuối.

**Header:** `<linux/timekeeping.h>` · **Hàm chính:** `ktime_get_real_seconds`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | #include <linux/timekeeping.h>; static time64_t last_write_time |
| Ⓐ write | last_write_time = ktime_get_real_seconds() |
| Ⓑ proc | In hiệu số |

**Code chính**

```c
/* lab2_write, trong mutex */
last_write_time = ktime_get_real_seconds();

/* lab2_proc_show */
if (last_write_time)
    seq_printf(m, "Last write    : %lld s ago\n",
               (long long)(ktime_get_real_seconds() - last_write_time));
else
    seq_printf(m, "Last write    : never\n");
```

**Test trong QEMU**

```sh
echo x > /dev/lab2 ; sleep 3 ; grep 'Last write' /proc/lab2_info     # 3 s ago
```

**Lỗi hay gặp**

- time64_t in bằng %d → sai trên ARM 32-bit (dùng %lld + ép kiểu long long)
- Dùng float để tính → không được dùng float trong kernel

### Bài 11. Thuộc tính sysfs GHI ĐƯỢC: mode ★★★

**Đề:** mode 0 = nguyên bản, 1 = chữ hoa, 2 = chữ thường; giá trị khác → -EINVAL.

**Header:** `<linux/device.h>`, `<linux/kernel.h>`, `<linux/ctype.h>` · **Hàm chính:** `DEVICE_ATTR_RW`, `kstrtoint`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | static int mode = 0; #include <linux/ctype.h> |
| Ⓐ write | Áp toupper/tolower theo mode (trong mutex) |
| Ⓑ sysfs | mode_show + mode_store + DEVICE_ATTR_RW + thêm vào mảng |

**Code chính**

```c
static ssize_t mode_store(struct device *d, struct device_attribute *a,
                          const char *buf, size_t count)
{
    int v;
    if (kstrtoint(buf, 10, &v) || v < 0 || v > 2)
        return -EINVAL;               /* kiểm tra trước … */
    mutex_lock(&lab2_mutex);
    mode = v;                         /* … rồi mới đổi */
    mutex_unlock(&lab2_mutex);
    return count;                     /* KHÔNG phải 0 */
}
static DEVICE_ATTR_RW(mode);          /* cần cả mode_show */
```

**Test trong QEMU**

```sh
echo 1 > /sys/class/lab2_class/lab2/mode ; echo Hi > /dev/lab2 ; cat /dev/lab2   # HI
echo 5 > /sys/class/lab2_class/lab2/mode            # Invalid argument
```

**Lỗi hay gặp**

- return 0 trong _store → echo treo
- Quên return ở cuối → lỗi -Werror=return-type
- copy_from_user trên buf của _store → sai, buf đã là bộ nhớ kernel

> Lời giải đầy đủ: [`ex03_sysfs_mode`](ex03_sysfs_mode/)

### Bài 12. Nút XOÁ qua sysfs (chỉ ghi) ★★☆

**Đề:** Ghi 1 vào /sys/class/lab2_class/lab2/clear thì xoá buffer và reset bộ đếm.

**Header:** `<linux/device.h>`, `<linux/kernel.h>` · **Hàm chính:** `DEVICE_ATTR_WO`, `kstrtoint`

| Vùng | Việc cần làm |
|---|---|
| Ⓑ sysfs | clear_store + DEVICE_ATTR_WO + thêm vào mảng |

**Code chính**

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
static DEVICE_ATTR_WO(clear);          /* quyền 0200, chỉ cần clear_store */
```

**Test trong QEMU**

```sh
echo abc > /dev/lab2 ; echo 1 > /sys/class/lab2_class/lab2/clear ; cat /dev/lab2   # rỗng
cat /sys/class/lab2_class/lab2/clear       # Permission denied (không có _show)
```

**Lỗi hay gặp**

- Viết thêm clear_show với DEVICE_ATTR_WO → warning unused
- Không kiểm tra v == 1 → echo gì cũng xoá

### Bài 13. Tham số khi nạp: max_len ★★☆

**Đề:** insmod lab2_driver.ko max_len=N; mặc định 1023, hợp lệ 1–1023, sai → insmod thất bại.

**Header:** `<linux/moduleparam.h>`, `<linux/kernel.h>` · **Hàm chính:** `module_param`, `MODULE_PARM_DESC`, `min_t`

| Vùng | Việc cần làm |
|---|---|
| ⓪ (sau #define) | static int max_len = BUFFER_SIZE - 1; module_param; MODULE_PARM_DESC |
| ⓘ đầu init | Kiểm tra trước register_chrdev |
| Ⓐ write | copy_len = min_t(size_t, len, max_len); return len |

**Code chính**

```c
static int max_len = BUFFER_SIZE - 1;
module_param(max_len, int, 0444);
MODULE_PARM_DESC(max_len, "Max bytes stored per write");

/* đầu lab2_init */
if (max_len < 1 || max_len > BUFFER_SIZE - 1) {
    pr_err("lab2_driver: invalid max_len=%d (must be 1..%d)\n", max_len, BUFFER_SIZE - 1);
    return -EINVAL;
}
```

**Test trong QEMU**

```sh
rmmod lab2_driver ; insmod /lib/modules/5.15.221/lab2_driver.ko max_len=5
echo abcdefgh > /dev/lab2 ; cat /dev/lab2               # abcde
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=0   # Invalid argument
```

**Lỗi hay gặp**

- MODULE_PARM_DES (thiếu C)
- <= / >= ở biên → loại nhầm 1 và 1023
- Mặc định 1024 → ghi '\0' ra ngoài mảng
- pr_info("%s", copy_len) → coi số là con trỏ

> Lời giải đầy đủ: [`ex04_module_param`](ex04_module_param/)

### Bài 14. Major do kernel tự cấp ★★☆

**Đề:** Dùng register_chrdev(0, …) để kernel chọn major còn trống.

**Header:** `<linux/fs.h>` · **Hàm chính:** `register_chrdev(0`, `…)`

| Vùng | Việc cần làm |
|---|---|
| ⓘ init | Lưu giá trị trả về làm major_number; dùng ở MKDEV và unregister_chrdev |

**Code chính**

```c
ret = register_chrdev(0, DEVICE_NAME, &lab2_fops);
if (ret < 0)
    return ret;
major_number = ret;                 /* major được cấp */
/* các chỗ sau vẫn dùng MKDEV(major_number, 0) */
```

**Test trong QEMU**

```sh
grep lab2 /proc/devices          # ví dụ 250 lab2
ls -l /dev/lab2                     # 250, 0 – devtmpfs tự tạo đúng số
```

**Lỗi hay gặp**

- Vẫn dùng DRIVER_MAJOR (240) ở chỗ khác → device_destroy sai
- mknod cứng 240 trong rcS → node sai (devtmpfs đã tạo đúng)

### Bài 15. Chỉ cho MỘT tiến trình mở ★★★

**Đề:** Lần open thứ hai khi đang có người mở → -EBUSY; đóng thì mở lại được.

**Header:** `<linux/atomic.h>` · **Hàm chính:** `atomic_cmpxchg`, `atomic_set`, `atomic_read`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | #include <linux/atomic.h>; static atomic_t in_use = ATOMIC_INIT(0) |
| Ⓐ open | atomic_cmpxchg(&in_use, 0, 1), khác 0 → -EBUSY |
| Ⓐ release | atomic_set(&in_use, 0) |

**Code chính**

```c
static atomic_t in_use = ATOMIC_INIT(0);

/* lab2_open */
if (atomic_cmpxchg(&in_use, 0, 1) != 0)
    return -EBUSY;                      /* đã có người giữ */
/* … open_count++ sau khi giành được */

/* lab2_release */
atomic_set(&in_use, 0);
```

**Test trong QEMU**

```sh
sleep 30 < /dev/lab2 &
cat /dev/lab2              # Device or resource busy
kill $! ; cat /dev/lab2    # mở được
```

**Lỗi hay gặp**

- Dùng int thường với if rồi gán → race trên 2 CPU
- Nhả ở cuối read thay vì release
- Thiếu } của if → hàng loạt lỗi invalid storage class

> Lời giải đầy đủ: [`ex05_single_open`](ex05_single_open/)

### Bài 16. Khoá NGẮT ĐƯỢC bằng Ctrl+C ★★☆

**Đề:** Thay mutex_lock bằng mutex_lock_interruptible trong read/write.

**Header:** `<linux/mutex.h>` · **Hàm chính:** `mutex_lock_interruptible`

| Vùng | Việc cần làm |
|---|---|
| Ⓐ read/write | Lấy khoá có thể bị signal ngắt; lỗi thì trả -ERESTARTSYS |

**Code chính**

```c
if (mutex_lock_interruptible(&lab2_mutex))
    return -ERESTARTSYS;          /* bị signal ngắt khi đang chờ – chưa lấy được khoá */
/* … */
mutex_unlock(&lab2_mutex);
```

**Test trong QEMU**

```sh
# test bình thường như base: echo / cat phải chạy giống hệt
```

**Lỗi hay gặp**

- goto out (có mutex_unlock) khi chưa lấy được khoá → unlock khoá không giữ
- Viết if (!mutex_lock_interruptible(...)) – ngược logic: trả 0 là THÀNH CÔNG

### Bài 17. Hàng đợi tin nhắn FIFO (ring buffer) ★★★

**Đề:** Lưu tối đa 8 tin; mỗi write đẩy 1 tin vào cuối, mỗi cat lấy 1 tin ở đầu; đầy → -ENOSPC.

**Header:** `<linux/uaccess.h>`, `<linux/kernel.h>` · **Hàm chính:** `copy_from_user`, `copy_to_user`, `min_t`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | char msgs[8][128], size_t msg_len[8], head, tail, count |
| Ⓐ write | Ghi vào msgs[tail], tail = (tail+1) % 8 |
| Ⓐ read | Lấy msgs[head], head = (head+1) % 8, một tin mỗi lần đọc |

**Code chính**

```c
#define MSG_MAX 8
#define MSG_LEN 128
static char   msgs[MSG_MAX][MSG_LEN];
static size_t msg_len[MSG_MAX];
static int    head, tail, count;

/* write (trong mutex) */
if (count == MSG_MAX) { ret = -ENOSPC; goto out; }
n = min_t(size_t, len, MSG_LEN);
if (copy_from_user(msgs[tail], buf, n)) { ret = -EFAULT; goto out; }
msg_len[tail] = n;
tail = (tail + 1) % MSG_MAX;
count++;
ret = len;

/* read (trong mutex) – mỗi lần mở chỉ lấy 1 tin */
if (*off > 0 || count == 0) { ret = 0; goto out; }
n = min_t(size_t, len, msg_len[head]);
if (copy_to_user(buf, msgs[head], n)) { ret = -EFAULT; goto out; }
head = (head + 1) % MSG_MAX;
count--;
*off += n;
ret = n;
```

**Test trong QEMU**

```sh
echo a > /dev/lab2 ; echo b > /dev/lab2
cat /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2      # a, b, (rỗng)
```

**Lỗi hay gặp**

- Không kiểm tra *off > 0 → cat lấy luôn tin thứ hai ở lần read kế tiếp
- Quên % MSG_MAX → chỉ số chạy ra ngoài mảng

### Bài 18. read CHẶN chờ dữ liệu (wait queue) ★★★

**Đề:** Khi buffer rỗng, cat đứng chờ cho đến khi có người ghi.

**Header:** `<linux/wait.h>` · **Hàm chính:** `DECLARE_WAIT_QUEUE_HEAD`, `wait_event_interruptible`, `wake_up_interruptible`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | #include <linux/wait.h>; DECLARE_WAIT_QUEUE_HEAD(lab2_wq) |
| Ⓐ read | wait_event_interruptible TRƯỚC khi lấy mutex |
| Ⓐ write | wake_up_interruptible SAU khi mở mutex |

**Code chính**

```c
static DECLARE_WAIT_QUEUE_HEAD(lab2_wq);

/* đầu lab2_read */
if (wait_event_interruptible(lab2_wq, buffer_len > 0))
    return -ERESTARTSYS;            /* Ctrl+C khi đang chờ */
mutex_lock(&lab2_mutex);
/* … logic read như cũ … */

/* cuối lab2_write, sau mutex_unlock */
wake_up_interruptible(&lab2_wq);
```

**Test trong QEMU**

```sh
# boot xong buffer rỗng
cat /dev/lab2 &            # đứng chờ
echo hi > /dev/lab2        # cat in ra "hi"
```

**Lỗi hay gặp**

- Chờ khi đang giữ mutex → write không lấy được khoá để đánh thức (deadlock)
- Quên wake_up → cat chờ mãi
- Điều kiện chờ sai (ví dụ buffer_len == 0) → không bao giờ chặn

### Bài 19. Lệnh ioctl: CLEAR và GET_LEN ★★★

**Đề:** LAB2_CLEAR xoá buffer; LAB2_GET_LEN trả buffer_len về userspace.

**Header:** `<linux/ioctl.h> / <sys/ioctl.h>`, `<linux/uaccess.h>` · **Hàm chính:** `_IO`, `_IOR`, `_IOC_TYPE`, `put_user`

| Vùng | Việc cần làm |
|---|---|
| header chung | Magic + 2 lệnh _IO/_IOR |
| Ⓐ ioctl | Hàm unlocked_ioctl + thêm vào fops |
| userspace | Chương trình C gọi ioctl, build -static |

**Code chính**

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

**Test trong QEMU**

```sh
arm-linux-gnueabihf-gcc -static -Wall -o lab2_app lab2_app.c      # trên laptop
lab2_app getlen ; lab2_app clear ; cat /dev/lab2                     # trong QEMU
```

**Lỗi hay gặp**

- Quên -static → not found
- Quên .unlocked_ioctl → Inappropriate ioctl for device
- Header hai bên khác nhau → mã lệnh lệch

> Lời giải đầy đủ: [`ASSIGNMENT1/task1_ioctl`](../ASSIGNMENT1/task1_ioctl/)

### Bài 20. Hai thiết bị /dev/lab2_0 và /dev/lab2_1 ★★★

**Đề:** Một driver, hai minor, mỗi thiết bị một buffer riêng.

**Header:** `<linux/fs.h>`, `<linux/device.h>`, `<linux/mutex.h>` · **Hàm chính:** `iminor`, `file->private_data`, `mutex_init`, `device_create`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | struct lab2_dev { buf, len, mutex } devs[2] |
| ⓘ init | mutex_init cho từng dev; device_create 2 lần (minor 0, 1) |
| Ⓐ open | iminor(inode) → file->private_data = &devs[minor] |
| Ⓐ read/write | Dùng file->private_data thay biến toàn cục |

**Code chính**

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
/* read/write: struct lab2_dev *d = file->private_data; dùng d->buf, d->len, d->lock */

/* init */
for (i = 0; i < NUM_DEV; i++) {
    mutex_init(&devs[i].lock);
    device_create(lab2_class, NULL, MKDEV(major_number, i), NULL, "lab2_%d", i);
}
/* exit: device_destroy(lab2_class, MKDEV(major_number, i)) cho từng i */
```

**Test trong QEMU**

```sh
echo A > /dev/lab2_0 ; echo B > /dev/lab2_1
cat /dev/lab2_0 ; cat /dev/lab2_1        # A  rồi  B
```

**Lỗi hay gặp**

- Quên mutex_init với mutex trong struct
- Dùng nhầm biến toàn cục cũ trong read/write
- rcS / test_driver.sh vẫn tìm /dev/lab2 → phải sửa

## 6. Bài bổ sung

| # | Bài | Độ khó |
|---|---|---|
| B1 | Mã hoá ROT13 | ★★☆ |
| B2 | Đảo hoa / thường | ★★☆ |
| B3 | Chế độ chỉ đọc (sysfs readonly) | ★★☆ |
| B4 | Đếm số từ và số dòng | ★★☆ |
| B5 | Kích thước buffer động (kmalloc) | ★★★ |
| B6 | Chuyển sang misc device | ★★☆ |
| B7 | Thời gian driver đã chạy (uptime) | ★★☆ |
| B8 | Hỗ trợ lseek (đọc từ vị trí bất kỳ) | ★★☆ |

### B1. Mã hoá ROT13 ★★☆

**Đề:** Lưu dữ liệu đã mã hoá ROT13 (dịch mỗi chữ cái 13 vị trí, giữ hoa/thường).

**Header:** `<linux/ctype.h>` · **Hàm chính:** `isalpha`, `islower`

| Vùng | Việc cần làm |
|---|---|
| Ⓐ write | Vòng lặp sau copy_from_user |

**Code chính**

```c
for (i = 0; i < len; i++) {
    char c = device_buffer[i];
    if (isalpha(c)) {
        char base = islower(c) ? 'a' : 'A';
        device_buffer[i] = base + (c - base + 13) % 26;
    }
}
```

**Test trong QEMU**

```sh
echo Hello > /dev/lab2 ; cat /dev/lab2      # Uryyb
```

**Lỗi hay gặp**

- Quên % 26 → ra ký tự ngoài bảng chữ

### B2. Đảo hoa / thường ★★☆

**Đề:** Chữ hoa thành thường, chữ thường thành hoa.

**Header:** `<linux/ctype.h>` · **Hàm chính:** `isupper`, `islower`, `toupper`, `tolower`

| Vùng | Việc cần làm |
|---|---|
| Ⓐ write | Vòng lặp sau copy_from_user |

**Code chính**

```c
for (i = 0; i < len; i++) {
    char c = device_buffer[i];
    if (isupper(c))      device_buffer[i] = tolower(c);
    else if (islower(c)) device_buffer[i] = toupper(c);
}
```

**Test trong QEMU**

```sh
echo HeLLo > /dev/lab2 ; cat /dev/lab2      # hEllO
```

**Lỗi hay gặp**

- Hai if rời → chữ vừa đổi sang hoa bị đổi lại

### B3. Chế độ chỉ đọc (sysfs readonly) ★★☆

**Đề:** Thuộc tính readonly (0/1); khi = 1 thì write trả -EPERM.

**Header:** `<linux/device.h>`, `<linux/kernel.h>` · **Hàm chính:** `DEVICE_ATTR_RW`, `kstrtoint`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | static int readonly |
| Ⓑ sysfs | readonly_show/store + DEVICE_ATTR_RW |
| Ⓐ write | Kiểm tra đầu hàm (trong mutex) |

**Code chính**

```c
/* đầu lab2_write, sau mutex_lock */
if (readonly) { ret = -EPERM; goto out; }
```

**Test trong QEMU**

```sh
echo 1 > /sys/class/lab2_class/lab2/readonly
echo hi > /dev/lab2          # Operation not permitted
```

**Lỗi hay gặp**

- _store chấp nhận giá trị khác 0/1

### B4. Đếm số từ và số dòng ★★☆

**Đề:** /proc/lab2_info hiện số từ (cụm ký tự không phải khoảng trắng) và số dòng ('\n').

**Header:** `<linux/ctype.h>` · **Hàm chính:** `isspace`

| Vùng | Việc cần làm |
|---|---|
| Ⓑ proc | Duyệt buffer, đếm chuyển trạng thái khoảng trắng → chữ |

**Code chính**

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

**Test trong QEMU**

```sh
printf 'one two\nthree\n' > /dev/lab2 ; grep Words /proc/lab2_info    # 3 / 2
```

**Lỗi hay gặp**

- Đếm khoảng trắng thay vì đếm lúc bắt đầu từ

### B5. Kích thước buffer động (kmalloc) ★★★

**Đề:** Tham số buf_size (64–4096); buffer cấp phát bằng kzalloc trong init.

**Header:** `<linux/slab.h>` · **Hàm chính:** `kzalloc`, `kfree`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | static char *device_buffer; static int buf_size = 1024; module_param |
| ⓘ init | kzalloc TRƯỚC register_chrdev; lỗi thì -ENOMEM |
| ⓘ exit | kfree CUỐI CÙNG |
| Mọi chỗ | Thay BUFFER_SIZE bằng buf_size |

**Code chính**

```c
device_buffer = kzalloc(buf_size, GFP_KERNEL);
if (!device_buffer)
    return -ENOMEM;
/* register_chrdev … nếu các bước sau lỗi: nhớ kfree(device_buffer) */

/* lab2_exit, sau unregister_chrdev */
kfree(device_buffer);
```

**Test trong QEMU**

```sh
rmmod lab2_driver ; insmod /lib/modules/5.15.221/lab2_driver.ko buf_size=64
head -c 200 /dev/zero | tr '\0' x > /dev/lab2 ; grep 'Data length' /proc/lab2_info   # 63
```

**Lỗi hay gặp**

- sizeof(device_buffer) giờ là 4 (con trỏ) chứ không phải kích thước buffer
- Quên kfree trong nhánh lỗi của init → rò bộ nhớ

### B6. Chuyển sang misc device ★★☆

**Đề:** Thay register_chrdev + class + device bằng miscdevice; giữ procfs và sysfs.

**Header:** `<linux/miscdevice.h>` · **Hàm chính:** `misc_register`, `misc_deregister`, `ATTRIBUTE_GROUPS`

| Vùng | Việc cần làm |
|---|---|
| ⓘ init/exit | misc_register / misc_deregister |
| Ⓑ sysfs | Dùng .groups của miscdevice |

**Code chính**

```c
ATTRIBUTE_GROUPS(lab2);               /* tạo lab2_groups từ mảng lab2_attrs[] */

static struct miscdevice lab2_misc = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "lab2",                  /* → /dev/lab2, major 10 */
    .fops   = &lab2_fops,
    .groups = lab2_groups,             /* sysfs: /sys/class/misc/lab2/… */
    .mode   = 0666,
};
/* init: ret = misc_register(&lab2_misc);   exit: misc_deregister(&lab2_misc); */
```

**Test trong QEMU**

```sh
ls -l /dev/lab2                         # 10, <minor>
cat /sys/class/misc/lab2/buffer_len
```

**Lỗi hay gặp**

- Vẫn gọi sysfs_create_group trên lab2_device (không còn tồn tại)
- Đường dẫn sysfs đổi sang /sys/class/misc/

### B7. Thời gian driver đã chạy (uptime) ★★☆

**Đề:** /proc/lab2_info hiện số giây kể từ lúc insmod.

**Header:** `<linux/jiffies.h>` · **Hàm chính:** `jiffies`, `jiffies_to_msecs`

| Vùng | Việc cần làm |
|---|---|
| ⓪ | static unsigned long load_jiffies |
| ⓘ init | load_jiffies = jiffies |
| Ⓑ proc | jiffies_to_msecs(jiffies - load_jiffies) / 1000 |

**Code chính**

```c
seq_printf(m, "Uptime        : %u s\n",
           jiffies_to_msecs(jiffies - load_jiffies) / 1000);
```

**Test trong QEMU**

```sh
sleep 5 ; grep Uptime /proc/lab2_info
```

**Lỗi hay gặp**

- Chia jiffies cho 100 – sai nếu HZ khác (dùng jiffies_to_msecs)

### B8. Hỗ trợ lseek (đọc từ vị trí bất kỳ) ★★☆

**Đề:** Cho phép dd skip= / lseek đổi vị trí đọc.

**Header:** `<linux/fs.h>` · **Hàm chính:** `default_llseek`

| Vùng | Việc cần làm |
|---|---|
| Ⓐ fops | .llseek = default_llseek |

**Code chính**

```c
static struct file_operations lab2_fops = {
    /* … */
    .llseek = default_llseek,
};
```

**Test trong QEMU**

```sh
echo "Hello World" > /dev/lab2
dd if=/dev/lab2 bs=1 skip=6 2>/dev/null       # World
```

**Lỗi hay gặp**

- read không dùng *offset làm điểm bắt đầu → seek không có tác dụng

## 7. Đề yêu cầu đặt tên riêng

| Tên yêu cầu | Sửa ở | Thấy ở |
|---|---|---|
| Module | Tên file `.c` **và** dòng `obj-m` của Makefile | `lsmod`, `insmod X.ko`, `rmmod X` |
| Device | `#define DEVICE_NAME` | `/dev/…`, `/proc/devices` |
| Class | `#define CLASS_NAME` | `/sys/class/…` |
| File proc | Chuỗi trong `proc_create("…", …)` | `/proc/…` |
| Major | `#define DRIVER_MAJOR` (241–254) | `ls -l /dev/…` |

> ⚠️ `rcS` luôn nạp `lab2_driver.ko` và chiếm major 240 → driver đổi tên mà vẫn 240 sẽ báo `Device or resource busy`. Driver đổi tên không được `rcS` tự nạp – phải tự `insmod`.

```bash
mv lab2_driver.c mydev.c
sed -i 's/lab2_driver\.o/mydev.o/' Makefile
# sửa tay: DEVICE_NAME, CLASS_NAME, "lab2_info", DRIVER_MAJOR
grep -n '"lab2\|240\|lab2_info' mydev.c      # phải trống
```

Bài mẫu: [`ex02b_rename`](ex02b_rename/)

## 8. Mã lỗi trả về

| Mã | Thông báo user thấy | Trả khi |
|---|---|---|
| `-EFAULT` | Bad address | copy_*_user thất bại |
| `-EINVAL` | Invalid argument | Tham số sai (ngoài khoảng, không phải số) |
| `-EBUSY` | Device or resource busy | Thiết bị đang có người dùng |
| `-ENOSPC` | No space left on device | Buffer / hàng đợi đầy |
| `-EPERM` | Operation not permitted | Không cho phép thao tác (vd đang ở chế độ chỉ đọc) |
| `-ENOTTY` | Inappropriate ioctl for device | Lệnh ioctl không hỗ trợ |
| `-ENOMEM` | Out of memory | kmalloc trả NULL |
| `-EAGAIN` | Resource temporarily unavailable | Chưa có dữ liệu, mở non-blocking |
| `-ERESTARTSYS` | (bị signal ngắt) | wait_event_interruptible / mutex_lock_interruptible trả lỗi |
| `-ENODEV` | No such device | Minor không tồn tại |

## 9. Checklist trước khi gọi giảng viên

- [ ] `make` không có warning.
- [ ] Mọi `return` sau `mutex_lock` đều đi qua `mutex_unlock`.
- [ ] Dữ liệu user ↔ kernel chỉ qua `copy_to_user` / `copy_from_user` (buf của sysfs `_store` thì không cần).
- [ ] `read` cập nhật `*offset` và trả `len` (không phải `*offset`).
- [ ] `write` / `_store` trả về số byte đã nhận (`len` / `count`).
- [ ] Test đủ giá trị biên (0, 1, giữa, max, max+1) với bài có khoảng giá trị.
- [ ] Tạo gì trong init thì huỷ trong exit, thứ tự ngược lại; `rmmod` chạy được.
- [ ] Log đã lưu bằng `script`; giải thích được echo → syscall → VFS → fops → driver.
