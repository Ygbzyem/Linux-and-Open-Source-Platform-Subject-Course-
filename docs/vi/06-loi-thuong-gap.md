# 06 – Lỗi thường gặp và cách sửa

[English](../06-troubleshooting.md) | **Tiếng Việt** · [← Về trang chủ](../../README.vi.md)

Mọi lỗi dưới đây **đã thực sự xảy ra** trong quá trình làm các lab. Tra theo triệu chứng.

**Nguyên tắc đọc lỗi `make`:** tìm dòng có `error:` hoặc `***` **đầu tiên** – các dòng sau
thường chỉ là hệ quả. Dạng `file.c:38:5: error:` = file, dòng 38, cột 5.

---

## 1. Lỗi khi build (`make`)

| Triệu chứng | Nguyên nhân | Cách sửa |
|---|---|---|
| `KDIR…: No such file or directory. Stop.` | `KDIR` trong Makefile trỏ sai | Sửa thành `KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15.221`. Dùng `$(HOME)`, không dùng `~` |
| `No rule to make target 'X.o'` | Tên trong `obj-m` không khớp tên file `.c` | `grep obj-m Makefile` – sửa cho trùng **từng chữ** với tên file |
| `missing separator. Stop.` / `No targets` | Dòng lệnh trong Makefile dùng dấu cách thay vì **TAB** | `cat -A Makefile` – dòng lệnh phải bắt đầu bằng `^I`. Tạo lại bằng `printf '\t…'` |
| `error: 'ret' undeclared` | Dùng biến chưa khai báo | Thêm `ssize_t ret;` ở **đầu thân hàm** |
| `label 'out' used but not defined` | Có `goto out` nhưng thiếu nhãn | Thêm `out:` trước `mutex_unlock` cuối hàm |
| `'x' defined but not used` (warning) | Khai báo biến/hằng mà chưa dùng | Dùng nó, hoặc xoá đi |
| `implicit declaration of function 'toupper'` | Thiếu include | Thêm `#include <linux/ctype.h>` (tra header ở [05](05-kernel-api.md)) |
| `too many arguments to function 'class_create'` | Code viết cho kernel ≥ 6.4 | Kernel 5.15 dùng `class_create(THIS_MODULE, name)` |
| `make` không in gì, không có `.ko` | Chạy `make 2>&1 \| grep …` che mất lỗi thật | Chạy `make` không kèm `grep` để thấy toàn bộ |

---

## 2. Lỗi khi đóng gói / boot

| Triệu chứng | Nguyên nhân | Cách sửa |
|---|---|---|
| Trong QEMU: `xxx: not found` dù đã build | **Quên đóng gói lại initramfs** | Luôn chạy `dev.sh`; kiểm tra danh sách "Kiem tra trong goi" trước khi boot |
| Log driver trong QEMU vẫn là chữ cũ | Gõ `qemu-system-arm` trực tiếp → boot gói cũ | Chỉ dùng `dev.sh` sau khi sửa code |
| `asgn1_app: not found` dù có trong `/usr/bin` | Binary build **động** – rootfs không có thư viện | Build với `-static`; kiểm tra `file app` có `statically linked` |
| `dev.sh` chạy xong không in gì | File `dev.sh` bị rỗng (0 byte) | `wc -c tools/dev.sh`; khôi phục: `git checkout -- tools/dev.sh` |
| `LOI: khong thay Makefile` | Chạy `dev.sh` ở sai thư mục | `cd` vào thư mục driver có Makefile |
| `Warning: unable to open an initial console` | Rootfs thiếu `/dev/console` | `sudo mknod -m 600 $R/dev/console c 5 1` rồi đóng gói lại |

Kiểm tra theo chuỗi **source → `.ko` → initramfs** – bước nào không thấy chuỗi mới thì lỗi ở đó:

```bash
grep -n 'chuoi_moi' mydrv.c
strings mydrv.ko | grep 'chuoi_moi'
zcat ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  | cpio -i --to-stdout lib/modules/5.15.221/mydrv.ko 2>/dev/null | strings | grep 'chuoi_moi'
```

---

## 3. Lỗi khi nạp / chạy driver

| Triệu chứng | Nguyên nhân | Cách sửa |
|---|---|---|
| `insmod: … File exists` | Module đã được nạp (thường do `rcS`) | `rmmod tên` trước, hoặc `grep -q '^tên ' /proc/modules \|\| insmod …` |
| `insmod: … Invalid module format` | vermagic lệch – build với kernel khác | `modinfo x.ko \| grep vermagic` phải là `5.15.221 SMP mod_unload ARMv7 p2v8`; kiểm tra `KDIR` |
| `insmod: … Device or resource busy` | Major đã bị driver khác chiếm | `cat /proc/devices`, chọn major khác (241–254) |
| `cat /dev/x` in mãi không dừng | `read` không cập nhật `*offset` | Thêm `*offset += len;` |
| Lệnh thứ hai trên thiết bị bị **treo** | Một nhánh `return` quên `mutex_unlock` | Viết lại theo mẫu `goto out` |
| Ghi chuỗi dài: buffer chỉ còn **phần đuôi** | `write` trả về ít hơn `len` → shell gọi lại và ghi đè | `write` luôn trả về `len` (số byte user gửi), cắt bớt ở bên trong |
| Ghi chuỗi ngắn sau chuỗi dài: thấy rác cũ | Đặt `'\0'` ở độ dài **cũ** (`buf[buffer_len]` trước khi cập nhật) | Đặt `'\0'` theo độ dài **mới** |
| `ioctl: Inappropriate ioctl for device` | Chưa gắn `.unlocked_ioctl`, hoặc header hai bên lệch | Kiểm tra `fops`; build lại **cả** module và app |
| Struct nhận từ ioctl ra số rác | `sizeof` trong `_IOR` khác struct thật | Cùng một header cho cả hai bên |
| `echo 1 > /sys/.../attr` lặp vô hạn / lỗi | Hàm `_store` không trả về `count` | `return count;` |

---

## 4. Lỗi terminal / shell

| Triệu chứng | Nguyên nhân | Cách sửa |
|---|---|---|
| Hàng loạt `syntax error near unexpected token` | **Dán code C vào terminal** thay vì vào editor | Code C chỉ dán vào editor; terminal chỉ gõ lệnh |
| Xuất hiện file lạ: `-append`, `-kernel`, `EOF`, `echo` | Khối lệnh nhiều dòng bị dán hỏng | Xoá bằng `rm -- -append …` (dấu `--` để tên bắt đầu bằng `-` không bị hiểu là tuỳ chọn). Kiểm tra `git status` trước khi commit |
| Ký tự lạ `^[[200~` khi dán | Bracketed paste | `bind 'set enable-bracketed-paste off'` |
| `sudo: Authentication failed` dù gõ đúng | Bộ gõ tiếng Việt đang bật / Caps Lock | Chuyển bàn phím tiếng Anh, gõ tay (không dán) |
| `lsmod` ra hàng trăm module (nvidia, i915…) | Đang gõ trên **laptop**, không phải QEMU | Nhìn dấu nhắc: QEMU là `/ # ` |
| heredoc + `sudo` nuốt mất lệnh | Dán khối lớn có `sudo` ở giữa | Chạy `sudo -v` trước, rồi từng lệnh |

---

## 5. Lỗi git

| Triệu chứng | Nguyên nhân | Cách sửa |
|---|---|---|
| GitHub từ chối push (file > 100 MB) | Lỡ commit `.tar.gz` / source kernel | `git reset --soft origin/main`, thêm vào `.gitignore`, commit lại |
| Commit lẫn file `.ko`, `.o`, binary | Dùng `git add -A` khi chưa kiểm tra | Luôn `git status --short` trước; add theo đường dẫn cụ thể |
| `deleted: …` trong `git status` sau khi xoá file | File đã từng được commit | `git rm -- <file>` rồi commit |

---

## 6. ⚠️ Lỗi nguy hiểm – MTD trên laptop

Laptop có sẵn một thiết bị MTD thật: **chip flash chứa BIOS** (`/proc/mtd` → `mtd0: "BIOS"`).
Ghi/xoá nhầm vào nó có thể làm máy không khởi động được.

**Không bao giờ gõ cứng `mtd0`.** Luôn tìm số của NAND giả lập:

```bash
MTD=$(grep 'NAND simulator' /proc/mtd | head -1 | cut -d: -f1)
echo "$MTD"     # phải là mtd1 (hoặc số khác mtd0). Rỗng → DỪNG LẠI
```

---

**Đọc tiếp:** [07 – Vấn đáp](07-van-dap.md)
