# Bài 02b – Đổi tên driver theo đề ★★

[English](README.md) | **Tiếng Việt** · [← Danh sách bài](../README.vi.md)

## Đề

Chức năng giữ nguyên như `base`, với các tên:

| Thành phần | Yêu cầu |
|---|---|
| Module | `se204386_drv` |
| Device | `/dev/tungdev` |
| Class | `tung_class` |
| File proc | `/proc/tung_info` |
| Major | `242` |

## Sửa ở đâu

| Thành phần | Thay đổi |
|---|---|
| Module | Đổi tên `lab2_driver.c` → `se204386_drv.c` **và** `obj-m += se204386_drv.o` |
| Device | `#define DEVICE_NAME "tungdev"` |
| Class | `#define CLASS_NAME "tung_class"` |
| proc | `proc_create("tung_info", …)` |
| Major | `#define DRIVER_MAJOR 242` |

Kiểm tra: `grep -n '"lab2\|240\|lab2_info' se204386_drv.c` phải trống.

## Test (QEMU)

```sh
insmod /lib/modules/5.15.221/se204386_drv.ko     # rcS không tự nạp – phải tự insmod
lsmod                                             # lab2_driver + se204386_drv
grep -E '^240|^242' /proc/devices                 # 240 lab2, 242 tungdev
ls -l /dev/tungdev                                # 242, 0
cat /proc/tung_info
echo hello > /dev/tungdev ; cat /dev/tungdev
rmmod se204386_drv
```

Log: [`test_log.txt`](test_log.txt)

## Lỗi hay gặp

| Sai | Hậu quả |
|---|---|
| Đổi tên file mà quên `obj-m` (hoặc ngược lại) | `No rule to make target 'X.o'` |
| Giữ major 240 | `insmod: Device or resource busy` – `lab2_driver` do `rcS` nạp đang giữ 240 |
| Chờ `rcS` tự nạp | `rcS` chỉ nạp `lab2_driver.ko` |
| Giữ `test_driver.sh` | Script đó test `/dev/lab2`, không phải device đã đổi tên |

## Câu hỏi vấn đáp

1. Tên module lấy từ đâu? → Tên file nguồn (`X.c` → `X.ko`).
2. Major number là gì? → Chỉ số trong bảng char driver của kernel; VFS đọc nó từ inode của file thiết
   bị rồi gọi `file_operations` của driver đó. Minor chọn thiết bị bên trong driver.
3. Vì sao hai driver không dùng chung major 240? → Mỗi ô trong bảng chứa một `file_operations`;
   `register_chrdev(240, …)` lần thứ hai trả `-EBUSY`.
