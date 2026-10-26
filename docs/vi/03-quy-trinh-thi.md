# 03 – Quy trình làm bài thi ⭐

[English](../03-exam-runbook.md) | **Tiếng Việt** · [← Về trang chủ](../../README.vi.md)

Đề thi dạng: *"Sửa driver để nó làm X, build, chạy trên QEMU và giải thích."*
Tài liệu này là **checklist từng bước**, copy-paste chạy được ngay.

---

## 0. Workflow 5 bước – thuộc lòng

```text
 LAPTOP (x86)                                              QEMU (ARM)
 ─────────────────────────────────────────────────────     ──────────────────────
 ① sửa .c  →  ② make  →  ③ chép vào rootfs  →  ④ đóng gói  →  ⑤ boot → insmod → test
                └──────────────── dev.sh làm ②③④⑤ ────────────────┘
```

| Bước | Quên thì bị gì |
|---|---|
| ② `make` | Không có `.ko` mới |
| ③ chép vào rootfs | QEMU chạy `.ko` cũ |
| ④ đóng gói | QEMU chạy **cả rootfs cũ** → `not found` |
| ⑤ nhìn đúng dấu nhắc | Gõ lệnh test trên laptop thay vì trong QEMU |

> 🚫 **Không gõ `qemu-system-arm` trực tiếp sau khi sửa code.** Luôn chạy `dev.sh`.

**Phân biệt dấu nhắc:**

| Dấu nhắc | Đang ở đâu |
|---|---|
| `tung_pham@tung-pham:~/...$` | Laptop |
| `/ # ` | Trong QEMU |

---

## 1. Kiểm tra môi trường (1 phút, làm ngay khi vào phòng thi)

```bash
ls ~/embedded_lab1/kernel/linux-5.15.221/Makefile
ls ~/embedded_lab1/output/zImage ~/embedded_lab1/output/vexpress-v2p-ca9.dtb
ls -l ~/embedded_lab2/dev.sh
arm-linux-gnueabihf-gcc --version | head -1
sudo -v
```

Lệnh nào báo `No such file` → xem [02 – Cài đặt môi trường](02-cai-dat-moi-truong.md).

> 💡 `sudo -v` nhập mật khẩu trước. Nếu sai: tắt bộ gõ tiếng Việt, kiểm tra Caps Lock.

---

## 2. Tạo thư mục bài làm từ driver gốc

Driver gốc = `LAB2/embedded_lab2/driver/lab2_driver.c` (đã chạy đúng Phần A + B).

```bash
REPO=~/Desktop/Linux-and-Open-Source-Platform-Subject-Course-
mkdir -p ~/exam && cp -r $REPO/LAB2/embedded_lab2/driver ~/exam/bai1
cd ~/exam/bai1
make clean >/dev/null 2>&1; ls
```

> 💡 **Giữ nguyên tên `lab2_driver`** nếu đề không yêu cầu đổi: `rcS` sẽ **tự `insmod`** bản mới
> lúc boot, và `test_driver.sh` vẫn chạy được. Nếu đổi tên/major → phải tự `insmod` và
> `mknod` theo major mới.

Nếu đề yêu cầu đổi tên driver:

```bash
mv lab2_driver.c mydrv.c
sed -i 's/lab2_driver\.o/mydrv.o/' Makefile
sed -i 's/lab2_/mydrv_/g; s/"lab2"/"mydrv"/' mydrv.c
grep -n 'lab2\|240' mydrv.c          # sửa tay những chỗ còn lại (major…)
```

---

## 3. Sửa code – đề nói gì thì sửa chỗ nào

Bản đồ file driver (chi tiết: [04 – Giải phẫu driver](04-giai-phau-driver.md)):

```text
 ⓪ #include, #define, biến toàn cục     ← thêm biến mới ở đây
 Ⓐ open / read / write / release + fops ← xử lý dữ liệu, đếm, đồng bộ
 Ⓑ procfs + sysfs                        ← hiển thị thông tin
 ⓘ init / exit                           ← đăng ký thêm attribute / proc / device
```

| Đề yêu cầu | Sửa ở | API chính |
|---|---|---|
| Biến đổi dữ liệu khi ghi (hoa, đảo, lọc…) | Ⓐ `write`, sau `copy_from_user` | `toupper`, `isalnum` (`<linux/ctype.h>`) |
| Biến đổi khi đọc | Ⓐ `read`, trước `copy_to_user`, dùng buffer tạm | |
| Đếm số lần đọc/ghi/mở | ⓪ thêm biến + Ⓐ tăng **trong mutex** | |
| Hiện thông tin qua `/proc` | Ⓑ `*_proc_show`, thêm `seq_printf` | `seq_printf` |
| Thuộc tính sysfs chỉ đọc | Ⓑ hàm `xxx_show` + `DEVICE_ATTR_RO(xxx)` + thêm vào mảng attrs | `sprintf` |
| Thuộc tính sysfs ghi được | Ⓑ `xxx_show` + `xxx_store` + `DEVICE_ATTR_RW(xxx)` | `kstrtoint` |
| Nút bấm sysfs (chỉ ghi) | Ⓑ `xxx_store` + `DEVICE_ATTR_WO(xxx)` | |
| Tham số khi `insmod` | ⓪ `module_param` | `module_param(name, int, 0444)` |
| Major do kernel tự cấp | ⓘ `register_chrdev(0, …)` và lưu giá trị trả về | |
| Chỉ cho 1 tiến trình mở | Ⓐ `open` trả `-EBUSY`, `release` nhả | `atomic_t`, `mutex_trylock` |
| `read` chờ đến khi có dữ liệu | Ⓐ wait queue | `wait_event_interruptible` |
| Lệnh điều khiển (ioctl) | Ⓐ hàm `unlocked_ioctl` + header chung + app userspace | `_IO/_IOR/_IOW` |

Tra chữ ký hàm: [05 – Kernel API](05-kernel-api.md).

**Mẫu bắt buộc khi thêm logic vào hàm có mutex** – một lối ra duy nhất:

```c
ssize_t ret;
mutex_lock(&lab2_mutex);
if (lỗi) { ret = -EFAULT; goto out; }
/* … */
ret = len;
out:
mutex_unlock(&lab2_mutex);
return ret;
```

---

## 4. Build và chạy

```bash
cd ~/exam/bai1
make 2>&1 | grep -E 'error|warning'     # phải không in gì
~/embedded_lab2/dev.sh
```

Đọc lỗi `make`: tìm dòng `file.c:SỐ_DÒNG:CỘT: error:` đầu tiên → mở file tới đúng dòng đó.
Bảng lỗi hay gặp: [06 – Lỗi thường gặp](06-loi-thuong-gap.md).

Trước khi QEMU boot, `dev.sh` in danh sách file trong gói – **phải thấy `.ko` của bài mình**.

---

## 5. Test trong QEMU – bộ lệnh theo loại đề

**Kiểm tra driver đã nạp:**

```sh
lsmod
grep lab2 /proc/devices
ls -l /dev/lab2
dmesg | tail -20
```

**Đọc / ghi:**

```sh
echo hello > /dev/lab2
cat /dev/lab2
cat /dev/lab2            # lần 2 – kiểm tra EOF/offset
```

**procfs / sysfs:**

```sh
cat /proc/lab2_info
ls -l /sys/class/lab2_class/lab2/
cat /sys/class/lab2_class/lab2/buffer_len
echo 1 > /sys/class/lab2_class/lab2/<thuoc_tinh_rw>
```

**Nạp / gỡ thủ công:**

```sh
rmmod lab2_driver
insmod /lib/modules/5.15.221/lab2_driver.ko
insmod /lib/modules/5.15.221/lab2_driver.ko max_len=5     # nếu có module_param
cat /sys/module/lab2_driver/parameters/max_len
```

**Ghi dữ liệu lớn / ký tự đặc biệt:**

```sh
head -c 2000 /dev/zero | tr '\0' x > /dev/lab2
printf 'abc' > /dev/lab2                  # không có \n
```

**Mở giữ file để thử single-open / blocking:**

```sh
sleep 30 < /dev/lab2 &       # tiến trình nền giữ file mở 30 giây
cat /dev/lab2                # thử mở lần 2
cat /dev/lab2 &              # đọc chặn ở nền
echo hi > /dev/lab2          # đánh thức
```

Thoát QEMU: **Ctrl+A rồi X**.

---

## 6. Lưu log để nộp / trình bày

Ghi toàn bộ phiên (cả phần trong QEMU) ra file:

```bash
cd ~/exam/bai1
script -q -c "$HOME/embedded_lab2/dev.sh" ~/exam/bai1/log_bai1.txt
```

Chạy test trong QEMU, rồi Ctrl+A X → log đã được lưu.

---

## 7. Checklist trước khi gọi giảng viên

- [ ] `make` không có `warning`
- [ ] Mọi đường `return` sau `mutex_lock` đều đi qua `mutex_unlock`
- [ ] Dữ liệu user ↔ kernel chỉ đi qua `copy_to_user` / `copy_from_user`
- [ ] `read` có cập nhật `*offset` → `cat` dừng được
- [ ] Cái gì tạo trong `init` thì được huỷ trong `exit`, **thứ tự ngược lại**
- [ ] `rmmod` chạy được, `lsmod` không còn module
- [ ] Giải thích được luồng: `echo` → syscall → VFS → `fops->write` → `copy_from_user`

---

## 8. Phân bổ thời gian gợi ý (đề 60 phút)

| Phút | Việc |
|---|---|
| 0–5 | Kiểm tra môi trường, tạo thư mục bài |
| 5–30 | Sửa code, build sạch |
| 30–45 | Test trong QEMU, sửa lỗi |
| 45–55 | Lưu log, rà checklist |
| 55–60 | Chuẩn bị giải thích (xem [07 – Vấn đáp](07-van-dap.md)) |

---

**Đọc tiếp:** [04 – Giải phẫu driver](04-giai-phau-driver.md)
