# Bài 03 – Thuộc tính sysfs ghi được `mode` ★★★

[English](README.md) | **Tiếng Việt** · [← Danh sách bài](../README.vi.md)

## Đề

Thêm thuộc tính đọc/ghi được `/sys/class/lab2_class/lab2/mode`:

| `mode` | Dữ liệu ghi vào `/dev/lab2` được lưu |
|---|---|
| `0` (mặc định) | Nguyên bản |
| `1` | CHỮ HOA |
| `2` | chữ thường |
| Khác | Từ chối, trả `-EINVAL` |

## Sửa ở đâu

| Vùng | Thay đổi |
|---|---|
| ⓪ | `#include <linux/ctype.h>`, `static int mode = 0;` |
| Ⓐ `lab2_write` | Sau `copy_from_user`: mode 1 thì `toupper`, mode 2 thì `tolower` (vẫn trong mutex) |
| Ⓑ sysfs | `mode_show`, `mode_store`, `static DEVICE_ATTR_RW(mode);` – đặt **sau** cả hai hàm |
| Ⓑ `lab2_attrs[]` | `&dev_attr_mode.attr,` trước `NULL` |

```c
static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    int v;

    if (kstrtoint(buf, 10, &v) || v < 0 || v > 2)
        return -EINVAL;              /* kiểm tra trước … */
    mutex_lock(&lab2_mutex);
    mode = v;                        /* … rồi mới đổi trạng thái */
    mutex_unlock(&lab2_mutex);
    return count;                    /* số byte đã nhận – KHÔNG phải 0 */
}
static DEVICE_ATTR_RW(mode);         /* 0644, cần mode_show + mode_store */
```

## Test (QEMU)

```sh
cat /sys/class/lab2_class/lab2/mode                      # 0
echo "Hello World" > /dev/lab2 ; cat /dev/lab2           # Hello World
echo 1 > /sys/class/lab2_class/lab2/mode
echo "Hello World" > /dev/lab2 ; cat /dev/lab2           # HELLO WORLD
echo 2 > /sys/class/lab2_class/lab2/mode
echo "Hello World" > /dev/lab2 ; cat /dev/lab2           # hello world
echo 5 > /sys/class/lab2_class/lab2/mode                 # write error: Invalid argument
echo abc > /sys/class/lab2_class/lab2/mode               # write error: Invalid argument
cat /sys/class/lab2_class/lab2/mode                      # 2 (không đổi)
ls -l /sys/class/lab2_class/lab2/mode                    # -rw-r--r--
```

Log: [`test_log.txt`](test_log.txt)

## Lỗi hay gặp (cả hai đều đã xảy ra khi làm bài này)

| Sai | Hậu quả |
|---|---|
| Không có `return` cuối `mode_store` | `error: control reaches end of non-void function` (kernel build với `-Werror=return-type`) |
| `return 0;` trong `mode_store` | `echo 1 > mode` **bị treo**: nhận 0 byte → echo ghi lại mãi (mode vẫn bị đổi) |
| Chép khối mode 1 làm mode 2 mà quên đổi `toupper` | Build qua, mode 2 vẫn ra chữ hoa – chỉ test từng nhánh mới phát hiện |
| Đặt `DEVICE_ATTR_RW` trước hai hàm | Lỗi `undeclared` |
| Quên thêm `&dev_attr_mode.attr` vào mảng | Build qua nhưng không có file `mode` trong sysfs |
| `copy_from_user` trên buffer của `_store` | Sai – sysfs đã chép sẵn vào bộ nhớ kernel (`const char *buf`, không có `__user`) |

## Câu hỏi vấn đáp

1. Vì sao `return count`? → Hàm ghi trả về số byte đã nhận; trả ít hơn thì bên gọi ghi lại.
2. Vì sao ở đây đọc `buf` trực tiếp được còn ở `lab2_write` thì không? → sysfs đưa cho `_store` buffer
   kernel; `write` nhận con trỏ `__user`.
3. Vì sao kiểm tra trước khi gán? → Dữ liệu sai thì trạng thái phải giữ nguyên.
4. sysfs hay ioctl? → sysfs là text, dùng được từ shell; ioctl cần chương trình C nhưng truyền được struct.
