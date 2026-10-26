# Bài 01 – Lưu dữ liệu dạng chữ hoa ★★

[English](README.md) | **Tiếng Việt** · [← Danh sách bài](../README.vi.md)

## Đề

Mọi chuỗi ghi vào `/dev/lab2` phải được **lưu** dưới dạng chữ hoa. Chữ số và ký tự đặc biệt
giữ nguyên.

## Sửa ở đâu

| Vùng | Thay đổi |
|---|---|
| ⓪ include | `#include <linux/ctype.h>` – `toupper` bản của kernel (kernel không có libc) |
| Ⓐ `lab2_write` | Vòng lặp trên `device_buffer` **sau** khi `copy_from_user` thành công, **trước** `mutex_unlock` |

```c
size_t i;
/* … sau khi copy_from_user thành công, vẫn đang giữ lab2_mutex … */
for (i = 0; i < len; i++)
    device_buffer[i] = toupper(device_buffer[i]);
```

## Test (QEMU)

```sh
echo "Hello Lab2 123!" > /dev/lab2
cat /dev/lab2                                  # HELLO LAB2 123!
grep 'Last data' /proc/lab2_info               # [HELLO LAB2 123!
cat /sys/class/lab2_class/lab2/last_data       # HELLO LAB2 123!
```

Log: [`test_log.txt`](test_log.txt)

## Lỗi hay gặp

| Sai | Vì sao sai |
|---|---|
| Đổi trên `buf[i]` | `buf` là con trỏ `__user` – không bao giờ dereference trong kernel |
| Đổi trong `lab2_read` | `cat` ra chữ hoa, nhưng `/proc` và sysfs vẫn hiện chữ thường đang được lưu |
| `i <= len` | Chạm thêm 1 byte ngoài dữ liệu; chỉ "chạy được" vì dòng sau ghi `'\0'` đè lên ô đó |
| Vòng lặp sau `mutex_unlock` | Sửa dữ liệu dùng chung mà không giữ khoá |

## Câu hỏi vấn đáp

1. Vì sao `<linux/ctype.h>` mà không phải `<ctype.h>`? → Kernel không dùng libc.
2. Vì sao vòng lặp phải chạy sau `copy_from_user`? → Trước đó dữ liệu còn ở bộ nhớ user.
3. `echo "xin chào"` ra gì? → `XIN CHàO`: `toupper` làm theo từng byte và chỉ biết ASCII; các byte
   UTF-8 lớn hơn 127 giữ nguyên.
4. Lưu hay hiển thị: muốn **lưu** thì sửa `write`, chỉ muốn **hiển thị** thì sửa `read`.
