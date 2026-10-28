# Bài luyện driver (dựa trên LAB-02)

[English](README.md) | **Tiếng Việt** · [← Về trang chủ](../README.vi.md)

Dạng đề thi: sửa character driver LAB-02 để làm việc mới, build, chạy trên QEMU và giải thích.
Mỗi bài bắt đầu từ một bản chép mới của [`base/`](base/) và giữ tên module `lab2_driver`, nên
`rcS` tự nạp nó khi boot.

> 📘 Sổ tay đầy đủ (20 + 8 bài, bảng header ↔ hàm, mẫu code): [`HANDBOOK.vi.md`](HANDBOOK.vi.md)

## Quy trình mỗi bài

```bash
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
cp -r "$REPO/EXERCISES/base" "$REPO/EXERCISES/exNN_ten_bai"
cd "$REPO/EXERCISES/exNN_ten_bai"
code lab2_driver.c                                   # sửa code
make 2>&1 | grep -E 'error|warning'                  # phải không in gì
script -q -c "$HOME/embedded_lab2/dev.sh" test_log.txt
```

Cách tiếp cận một đề mới: [`docs/vi/03-quy-trinh-thi.md`](../docs/vi/03-quy-trinh-thi.md).

## Danh sách bài

| # | Bài | Độ khó | Sửa ở | API chính | Trạng thái |
|---|---|---|---|---|---|
| 01 | [Lưu dữ liệu dạng chữ hoa](ex01_toupper/README.vi.md) | ★★ | `write` | `toupper` | ✅ |
| 02 | [Đếm số lần đọc và ghi](ex02_counters/README.vi.md) | ★★ | `read`, `write`, procfs | mutex, `goto out`, `seq_printf` | ✅ |
| 02b | [Đổi tên driver theo đề](ex02b_rename/README.vi.md) | ★★ | tên, major | `obj-m`, `DEVICE_NAME`, `CLASS_NAME` | ✅ |
| 03 | [Thuộc tính sysfs ghi được `mode`](ex03_sysfs_mode/README.vi.md) | ★★★ | sysfs, `write` | `DEVICE_ATTR_RW`, `kstrtoint` | ✅ |
| 04 | [Tham số khi nạp `max_len`](ex04_module_param/README.vi.md) | ★★ | `module_param`, `init`, `write` | `module_param`, `min_t` | ✅ |
| 05 | [Chỉ cho một tiến trình mở](ex05_single_open/README.vi.md) | ★★★ | `open`, `release` | `atomic_cmpxchg`, `-EBUSY` | ✅ |

> Luyện xong, khôi phục driver gốc trong rootfs:
> `cd "$REPO/EXERCISES/base" && ~/embedded_lab2/dev.sh`
