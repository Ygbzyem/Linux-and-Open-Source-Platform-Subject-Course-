# Bài 02 – Đếm số lần đọc và ghi ★★

[English](README.md) | **Tiếng Việt** · [← Danh sách bài](../README.vi.md)

## Đề

Thêm hai bộ đếm và hiện trong `/proc/lab2_info`:

- `write_count` – +1 mỗi lần ghi **thành công**
- `read_count` – +1 mỗi lần `read` **trả về dữ liệu** (lần trả 0 – EOF – không tính)

## Sửa ở đâu

| Vùng | Thay đổi |
|---|---|
| ⓪ biến toàn cục | `static int write_count = 0;` và `static int read_count = 0;` cạnh `open_count` |
| Ⓐ `lab2_write` | `write_count++` sau khi `copy_from_user` thành công, trước `mutex_unlock` |
| Ⓐ `lab2_read` | Lấy mutex, một lối ra qua `goto out`, `read_count++` ở nhánh có dữ liệu |
| Ⓑ `lab2_proc_show` | Hai dòng `seq_printf` |

`lab2_read` theo mẫu một lối ra:

```c
static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    if (*offset >= buffer_len) { ret = 0; goto out; }          /* EOF – không đếm */
    if (len > buffer_len - *offset)
        len = buffer_len - *offset;
    if (copy_to_user(buf, device_buffer + *offset, len)) { ret = -EFAULT; goto out; }
    *offset += len;
    read_count++;
    ret = len;                       /* số byte gửi trong LẦN NÀY, không phải vị trí */
out:
    mutex_unlock(&lab2_mutex);
    return ret;
}
```

## Test (QEMU)

```sh
echo hi > /dev/lab2 ; cat /dev/lab2 ; cat /dev/lab2
grep count /proc/lab2_info          # Open 3, Write 1, Read 2
echo -n abc > /dev/lab2 ; dd if=/dev/lab2 bs=1 2>/dev/null
grep count /proc/lab2_info          # Open 5, Write 2, Read 5
```

`dd bs=1` đọc mỗi lần 1 byte: 3 lần trả dữ liệu (+3), 1 lần trả 0 (không tính).

Log: [`test_log.txt`](test_log.txt)

## Lỗi hay gặp

| Sai | Hậu quả |
|---|---|
| Có `mutex_unlock` mà không có `mutex_lock` | Mở khoá không giữ làm hỏng khoá – hai tiến trình có thể cùng vào vùng găng |
| `ret = *offset` thay vì `ret = len` | Chỉ trùng ở lần gọi đầu; với `dd bs=1` bên đọc nhận byte rác |
| Khai báo bộ đếm bên trong hàm | Bị tạo lại và về 0 mỗi lần gọi |
| `read_count++` ở đầu hàm `read` | Đếm cả lần EOF → mỗi `cat` tính 2 |
| Quên `seq_printf` | Bộ đếm vẫn chạy nhưng không hiện ra |

## Câu hỏi vấn đáp

1. Vì sao `count++` phải nằm trong mutex? → `++` gồm đọc–cộng–ghi; hai CPU xen kẽ sẽ mất một lần đếm.
2. `cat` gọi `read` mấy lần với buffer 3 byte? → Hai lần: 3 byte, rồi 0 (EOF).
3. `read` trả về gì? → Số byte chép trong lần này; 0 = EOF; số âm = lỗi.
