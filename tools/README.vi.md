# tools/

[English](README.md) | **Tiếng Việt** · [← Về trang chủ](../README.vi.md)

## `dev.sh` – build → chép → đóng gói → boot

Chạy trong **thư mục có Makefile** của driver:

```bash
cd ASSIGNMENT1/task1_ioctl
../../tools/dev.sh
# hoặc qua symlink:  ~/embedded_lab2/dev.sh
```

| Bước | Việc | Chi tiết |
|---|---|---|
| — | Kiểm tra | Dừng ngay nếu thư mục hiện tại không có `Makefile` |
| — | `sudo -v` | Nhập mật khẩu một lần cho cả script |
| [2] | Build | `make` (dừng nếu lỗi nhờ `set -e`) |
| [3] | Chép vào rootfs | `*.ko` → `lib/modules/5.15.221/`; `*.sh` → `usr/bin/`; mọi binary **ARM static** → `usr/bin/` |
| [4] | Đóng gói | `find . \| cpio -H newc -o \| gzip` → `~/embedded_lab2/output/initramfs_lab2.cpio.gz`, in ra các `.ko` và script có trong gói |
| [5] | Boot | `qemu-system-arm -M vexpress-a9 …` (thoát: **Ctrl+A rồi X**) |

Biến cấu hình ở đầu file (sửa nếu đặt môi trường ở chỗ khác):

```bash
R=$HOME/embedded_lab2/rootfs/initramfs
OUT=$HOME/embedded_lab2/output/initramfs_lab2.cpio.gz
KVER=5.15.221
```

Ghi lại toàn bộ phiên làm log:

```bash
script -q -c "$HOME/embedded_lab2/dev.sh" log.txt
```

Cài symlink (một lần):

```bash
ln -sf "$PWD/tools/dev.sh" ~/embedded_lab2/dev.sh
```
