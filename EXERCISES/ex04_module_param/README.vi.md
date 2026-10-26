# Bài 04 – Tham số khi nạp `max_len` ★★

[English](README.md) | **Tiếng Việt** · [← Danh sách bài](../README.vi.md)

## Đề

Thêm tham số `max_len` truyền vào lúc `insmod`:

- Số byte tối đa được lưu mỗi lần ghi. Mặc định **1023**, hợp lệ **1–1023**.
- Ngoài khoảng → `insmod` thất bại với `-EINVAL`.
- Chuỗi dài hơn bị cắt bớt **nhưng không** bị lỗi ghi lặp.
- Hiện `max_len` trong `/proc/lab2_info`.

## Sửa ở đâu

| Vùng | Thay đổi |
|---|---|
| ⓪ biến toàn cục (sau các `#define`) | `static int max_len = BUFFER_SIZE - 1;` `module_param(max_len, int, 0444);` `MODULE_PARM_DESC(...)` |
| ⓘ đầu `lab2_init` | Kiểm tra **trước** `register_chrdev` – lỗi thì không phải dọn gì |
| Ⓐ `lab2_write` | Chép `copy_len = min_t(size_t, len, max_len)` byte, **trả về `len`** |
| Ⓑ `lab2_proc_show` | `Max length : %d` |

```c
static int __init lab2_init(void)
{
    if (max_len < 1 || max_len > BUFFER_SIZE - 1) {
        pr_err("lab2_driver: invalid max_len=%d (must be 1..%d)\n", max_len, BUFFER_SIZE - 1);
        return -EINVAL;
    }
    /* register_chrdev … */
}
```

## Thuộc tính sysfs vs tham số module

| | Bài 03: sysfs `mode` | Bài 04: `module_param` |
|---|---|---|
| Đặt khi nào | Bất cứ lúc nào, **đang chạy** | Một lần, lúc `insmod` |
| Cách đặt | `echo 1 > /sys/class/…/mode` | `insmod lab2_driver.ko max_len=5` |
| Xem ở đâu | `/sys/class/lab2_class/lab2/mode` | `/sys/module/lab2_driver/parameters/max_len` |
| Đổi giá trị | `echo` lại | `rmmod` rồi `insmod` |

## Test (QEMU)

```sh
cat /sys/module/lab2_driver/parameters/max_len        # 1023 (rcS nạp không tham số)
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=5
echo abcdefgh > /dev/lab2 ; cat /dev/lab2              # abcde
dmesg | tail -4                                         # MỘT dòng "truncated", MỘT dòng "received"
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=1  # chấp nhận (biên dưới)
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=0     # Invalid argument
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=1024  # Invalid argument
```

Log: [`test_log.txt`](test_log.txt)

## Lỗi hay gặp (tất cả đều đã xảy ra khi làm bài này)

| Sai | Hậu quả |
|---|---|
| `MODULE_PARM_DES` | Lỗi build – macro đúng là `MODULE_PARM_DESC` |
| `pr_err();` không có chuỗi | Lỗi build; và log không kèm giá trị thì không biết nguyên nhân |
| `pr_info("[%s]", copy_len)` | `%s` coi số là con trỏ → có thể kernel oops |
| `max_len <= 1 && max_len >= 1023` | `&&` không bao giờ đúng → không kiểm tra gì cả |
| `max_len <= 1 \|\| max_len >= 1023` | Loại nhầm biên hợp lệ 1 và 1023 → mặc định bị từ chối, driver không nạp khi boot |
| Mặc định 1024 | `device_buffer[1024] = '\0'` ghi ra ngoài mảng 1024 phần tử |
| `write` trả về `copy_len` | Shell ghi lại phần còn thiếu → buffer chỉ còn phần đuôi |

Luôn test đủ 5 điểm biên: 0, 1, giữa khoảng, 1023, 1024.

## Câu hỏi vấn đáp

1. `max_len` được gán lúc nào? → Kernel gán từ dòng lệnh `insmod`, **trước** khi `lab2_init` chạy.
2. Vì sao kiểm tra trước `register_chrdev`? → Chưa tạo gì nên chỉ cần `return -EINVAL`, không phải dọn dẹp.
3. `0444` nghĩa là gì ở đây? → `/sys/module/lab2_driver/parameters/max_len` chỉ đọc khi đang chạy.
4. Vì sao sau khi sửa, `rcS` có thể không nạp được driver? → `rcS` nạp không kèm tham số; nếu giá trị mặc định sai thì `init` từ chối.
