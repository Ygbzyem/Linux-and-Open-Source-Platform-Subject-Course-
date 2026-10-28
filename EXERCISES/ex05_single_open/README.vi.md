# Bài 05 – Chỉ cho một tiến trình mở ★★★

[English](README.md) | **Tiếng Việt** · [← Danh sách bài](../README.vi.md)

## Đề

Tại một thời điểm chỉ một tiến trình được mở `/dev/lab2`. Lần `open` thứ hai trả **`-EBUSY`**;
khi tiến trình đầu đóng file thì mở lại được. `open_count` chỉ đếm lần mở thành công.
`/proc/lab2_info` hiện `In use : yes/no`.

## Sửa ở đâu

| Vùng | Thay đổi |
|---|---|
| ⓪ | `#include <linux/atomic.h>`, `static atomic_t in_use = ATOMIC_INIT(0);` |
| Ⓐ `lab2_open` | `atomic_cmpxchg(&in_use, 0, 1)`; giá trị cũ ≠ 0 → `return -EBUSY`; chỉ đếm sau khi giành được |
| Ⓐ `lab2_release` | `atomic_set(&in_use, 0);` |
| Ⓑ `lab2_proc_show` | `In use : %s` với `atomic_read(&in_use) ? "yes" : "no"` |

```c
static int lab2_open(struct inode *inode, struct file *file)
{
    if (atomic_cmpxchg(&in_use, 0, 1) != 0) {   /* kiểm tra và gán trong MỘT bước */
        pr_info("lab2_driver: busy\n");
        return -EBUSY;
    }
    mutex_lock(&lab2_mutex);
    open_count++;                              /* chỉ đếm lần mở thành công */
    mutex_unlock(&lab2_mutex);
    return 0;
}

static int lab2_release(struct inode *inode, struct file *file)
{
    atomic_set(&in_use, 0);
    return 0;
}
```

## Test (QEMU)

```sh
sleep 30 < /dev/lab2 &               # tiến trình nền giữ thiết bị mở
grep -E 'Open|In use' /proc/lab2_info   # Open count 1, In use yes
cat /dev/lab2                        # can't open '/dev/lab2': Device or resource busy
echo hi > /dev/lab2                  # can't create /dev/lab2: Device or resource busy
kill $!                              # tiến trình bị kill → release vẫn chạy
grep -E 'Open|In use' /proc/lab2_info   # In use no, Open count vẫn 1
echo hi > /dev/lab2 ; cat /dev/lab2  # hi
```

Log: [`test_log.txt`](test_log.txt)

## Lỗi hay gặp

| Sai | Hậu quả |
|---|---|
| `int busy; if (busy) return -EBUSY; busy = 1;` | Kiểm tra và gán là hai bước → hai CPU cùng lọt vào |
| Thiếu `}` của `if` trong `open` | Hàng loạt lỗi `invalid storage class for function` cho mọi hàm phía sau |
| `atomic_cmpxchg` 2 tham số trong `release` | Lỗi build – release chỉ cần `atomic_set(&in_use, 0)` |
| Nhả ở cuối `read`/`write` | Tiến trình khác mở được khi file vẫn đang mở |
| `open_count++` trước khi kiểm tra | Lần bị từ chối cũng bị đếm |

## Câu hỏi vấn đáp

1. Vì sao dùng `atomic_cmpxchg`? → So sánh và gán trong một thao tác không chia cắt; trả về giá trị cũ.
2. Tiến trình giữ thiết bị bị kill thì thiết bị có bị khoá mãi? → Không – kernel đóng mọi file của nó nên `release` vẫn chạy.
3. Vì sao `test_driver.sh` vẫn PASS? → Các lần mở nối tiếp nhau, không chồng lên nhau.
