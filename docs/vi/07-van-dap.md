# 07 – Câu hỏi vấn đáp

[English](../07-viva-qa.md) | **Tiếng Việt** · [← Về trang chủ](../../README.vi.md)

Mỗi câu có **trả lời ngắn** (để nói miệng trong 20–30 giây) và **giải thích sâu** (khi bị hỏi vặn).

---

## A. Hệ thống Embedded Linux

### A1. Bốn thành phần của một hệ thống Embedded Linux?

**Ngắn:** Toolchain, bootloader, kernel, root filesystem.

**Sâu:** Toolchain (`arm-linux-gnueabihf-gcc`) nằm trên máy host, dùng để build ba thứ còn
lại. Bootloader (U-Boot) khởi tạo RAM và nạp kernel. Kernel quản lý phần cứng, tiến trình,
bộ nhớ. Rootfs (BusyBox) chứa `/sbin/init` và các lệnh người dùng.

### A2. Kể quá trình từ lúc bật máy đến dấu nhắc `/ #`.

**Ngắn:** QEMU nạp kernel + dtb + initramfs → kernel khởi động, giải nén initramfs →
chạy `/sbin/init` → đọc `/etc/inittab` → chạy `rcS` → mở shell.

**Sâu:** `rcS` (action `::sysinit:`) mount `proc`, `sysfs`, `devtmpfs` rồi `insmod` driver;
sau đó `::askfirst:-/bin/sh` in "Please press Enter" và mở shell. Trên board thật, trước kernel
còn có ROM code và U-Boot.

### A3. Device Tree để làm gì?

**Ngắn:** Mô tả phần cứng (RAM ở đâu, UART ở địa chỉ nào…) cho kernel, vì phần cứng ARM
không tự khai báo được như PCI trên PC.

**Sâu:** Kernel so khớp chuỗi `compatible` trong `.dtb` với driver. Nhờ đó một `zImage` dùng
được cho nhiều board, chỉ cần đổi `.dtb`.

### A4. Vì sao chương trình C tự viết phải build `-static`?

**Ngắn:** Rootfs BusyBox static không có thư viện động (`libc.so`). Binary động sẽ báo
`not found` vì không tìm thấy dynamic loader.

---

## B. Kernel module & driver

### B1. Major number là gì?

**Ngắn:** Số định danh driver. Khi mở `/dev/lab2` (major 240), VFS dùng 240 để tra ra
`file_operations` mà `register_chrdev(240, …)` đã đăng ký.

**Sâu:** Minor phân biệt các thiết bị cùng một driver. `ls -l /dev/lab2` → `240, 0`.

### B2. `file_operations` là gì?

**Ngắn:** Bảng con trỏ hàm driver giao cho kernel: syscall `read` trên file thiết bị sẽ được
VFS chuyển tới `fops->read`.

### B3. Thứ tự trong `init` và `exit`? Vì sao `exit` ngược lại?

**Ngắn:** init: `register_chrdev` → `class_create` → `device_create`. exit làm ngược lại vì
thứ tạo sau phụ thuộc thứ tạo trước – device nằm trong class, nên phải huỷ device trước class.

### B4. Nếu `class_create` thất bại trong `init` thì sao?

**Ngắn:** Phải `unregister_chrdev` (huỷ bước đã thành công) rồi trả mã lỗi. Nếu không, major
bị chiếm vĩnh viễn và lần `insmod` sau báo `Device or resource busy`.

### B5. vermagic là gì? Vì sao `insmod` báo `Invalid module format`?

**Ngắn:** Chuỗi "dấu vân tay" của kernel (`5.15.221 SMP mod_unload ARMv7 p2v8`). Module chỉ nạp
được vào kernel có vermagic khớp → phải build module với **chính source tree** đã build kernel
(`KDIR` trong Makefile).

### B6. `devtmpfs` liên quan gì tới `/dev/lab2`?

**Ngắn:** Khi `device_create()` chạy, kernel tự tạo node `/dev/lab2` trong devtmpfs – không cần
`mknod`. Vì vậy `rcS` dùng `[ -c /dev/lab2 ] || mknod …` để không lỗi `File exists`.

---

## C. Bộ nhớ user ↔ kernel

### C1. Vì sao dùng `copy_to_user` mà không dùng `memcpy` hay dereference con trỏ trực tiếp? (Task 4 – Câu 2)

**Ngắn:** Con trỏ từ userspace là **địa chỉ ảo của tiến trình**, có thể sai, có thể trỏ vào
vùng kernel, hoặc trỏ vào trang chưa nằm trong RAM. `copy_*_user` kiểm tra địa chỉ và xử lý
lỗi trang an toàn; `memcpy` thì không.

**Sâu (cấp MMU):**

- Mỗi tiến trình có bảng trang riêng; MMU dịch địa chỉ ảo → vật lý. Địa chỉ user chỉ có nghĩa
  trong ngữ cảnh tiến trình đó.
- `copy_*_user` kiểm tra địa chỉ có nằm trong vùng user không (`access_ok`). Không có bước này,
  một chương trình độc hại có thể truyền địa chỉ kernel để đọc/ghi bộ nhớ kernel.
- Trang user có thể chưa được nạp (lazy allocation, swap). Truy cập sẽ gây **page fault**.
  `copy_*_user` dùng bảng ngoại lệ (exception table): fault ở địa chỉ hợp lệ thì kernel nạp
  trang rồi chạy tiếp; ở địa chỉ không hợp lệ thì hàm trả về số byte chưa chép → driver trả
  `-EFAULT`. `memcpy` gặp fault đó sẽ gây **kernel oops**.
- Trên ARM có thể bật PAN (Privileged Access Never): kernel bị cấm truy cập trực tiếp bộ nhớ
  user; `copy_*_user` mở tạm quyền này một cách có kiểm soát.

### C2. `copy_to_user` trả về gì?

**Ngắn:** Số byte **không** chép được. 0 = thành công. Khác 0 → trả `-EFAULT`.

### C3. Khi nào `memcpy` trong driver là đúng?

**Ngắn:** Khi **cả hai** đầu đều là bộ nhớ kernel – ví dụ chép tiền tố `"[KERNEL] "` (hằng
trong module) vào `device_buffer`.

---

## D. Đồng bộ

### D1. Mutex trong driver để làm gì?

**Ngắn:** Hai tiến trình cùng `write` (hoặc một `write`, một `read`) sẽ cùng đụng vào `buffer`
và `buffer_len` → race condition. Mutex đảm bảo mỗi lúc chỉ một tiến trình ở trong vùng găng.
Tương tự `xSemaphoreTake` trong FreeRTOS.

### D2. Vì sao mở mutex **trước** khi `copy_to_user` trong ioctl GET_STATS?

**Ngắn:** `copy_to_user` có thể gây page fault và làm tiến trình **ngủ**. Giữ mutex trong lúc
ngủ sẽ chặn mọi tiến trình khác vô ích. Cách làm: chép dữ liệu ra struct cục bộ trong lúc giữ
khoá → mở khoá → `copy_to_user`.

### D3. Vì sao `memset(&st, 0, sizeof(st))` trước khi trả struct ra user?

**Ngắn:** Struct cục bộ nằm trên stack kernel, có thể chứa rác (kể cả byte đệm giữa các trường).
Không xoá → **rò rỉ dữ liệu kernel** ra userspace.

### D4. Mutex hay atomic?

**Ngắn:** `atomic_t` cho **một** biến đếm/cờ đơn. Mutex khi **nhiều** biến phải đổi cùng nhau
(buffer + len) hoặc thao tác có thể ngủ.

### D5. Vì sao dùng mẫu `goto out`?

**Ngắn:** Hàm có nhiều nhánh lỗi; mỗi nhánh tự `mutex_unlock` thì rất dễ quên một chỗ → lần gọi
sau treo vĩnh viễn. `goto out` cho một lối ra duy nhất, `mutex_unlock` chỉ viết một lần.

---

## E. read / write / offset

### E1. `*ppos` (offset) trong `read` là gì? VFS gọi `read` lặp như thế nào? (Task 4 – Câu 4)

**Ngắn:** `*ppos` là vị trí đọc hiện tại của **lần mở file đó**, VFS lưu trong `struct file`
và truyền vào mỗi lần gọi. Driver đọc từ `*ppos`, rồi tăng nó thêm số byte đã trả. `cat` gọi
`read()` lặp lại cho tới khi nhận **0** (EOF) thì dừng.

**Sâu:**

```text
cat /dev/asgn1   (buffer 6 byte)
  read(fd, buf, 4096)  ppos=0 → driver trả 6, ppos=6
  read(fd, buf, 4096)  ppos=6 → *ppos >= buffer_len → trả 0 → cat dừng
```

Điều kiện dừng do **driver** quyết định: trả 0. Không cập nhật `*ppos` → lần nào cũng trả dữ
liệu từ đầu → vòng lặp vô hạn. Mỗi lần `open` mới thì `ppos` bắt đầu lại từ 0.

### E2. `write` trả về ít hơn `len` thì chuyện gì xảy ra?

**Ngắn:** Userspace hiểu là "mới ghi được một phần" và gọi `write` lần nữa với phần còn lại.
Với driver ghi đè từ đầu buffer, phần đuôi sẽ đè mất phần đầu. Đã quan sát thực tế:
`received 1023` rồi `received 977`, buffer chỉ còn 977 byte.

---

## F. Module reference count (Task 4 – Câu 1)

### F1. Reference count của module tăng/giảm khi nào?

**Ngắn:** Tăng khi có thứ đang **dùng** code của module – điển hình là một tiến trình đang mở
`/dev/asgn1`; giảm khi thứ đó thôi dùng (đóng file).

**Sâu:** Nhờ `.owner = THIS_MODULE` trong `file_operations`, VFS tự gọi `try_module_get()` khi
`open` thành công và `module_put()` khi `release`. Module khác dùng symbol của module này cũng
làm tăng count. Xem cột thứ 3 của `lsmod`.

### F2. `rmmod` khi một tiến trình đang mở thiết bị thì sao?

**Ngắn:** Bị từ chối vì refcount > 0 (BusyBox báo dạng `rmmod: can't unload module
'asgn1_driver': …` – ghi lại thông báo thật khi thử). Nếu không có cơ chế này, kernel sẽ gỡ code trong khi tiến trình
vẫn có thể gọi `read` → nhảy vào vùng nhớ đã giải phóng → oops.

Thử:

```sh
sleep 60 < /dev/asgn1 &
lsmod                 # cột "used by" = 1
rmmod asgn1_driver    # bị từ chối
kill %1; rmmod asgn1_driver   # được
```

### F3. Ép gỡ module thế nào?

**Ngắn:** Cách đúng: tìm và dừng tiến trình đang giữ file (`kill`) rồi `rmmod`. Cách ép:
`rmmod -f`, chỉ chạy được nếu kernel bật `CONFIG_MODULE_FORCE_UNLOAD` – **rất nguy hiểm**, vì
code bị gỡ trong khi vẫn còn người dùng → dễ crash kernel. Chỉ dùng khi debug.

---

## G. MTD và JFFS2 (Task 4 – Câu 3)

### G1. MTD khác block device thế nào?

**Ngắn:** Block device (SSD, thẻ SD, USB) có **bộ điều khiển riêng** lo việc xoá, cân bằng
hao mòn, quản lý block hỏng → hệ điều hành chỉ thấy các sector đọc/ghi tuỳ ý. MTD là flash
**thô**, không có bộ điều khiển đó → kernel phải tự xử lý.

| | Block device | MTD (raw flash) |
|---|---|---|
| Thao tác | read / write sector | read / write page / **erase block** |
| Ghi đè | Được | Không – phải xoá cả erase block trước (bit chỉ 1 → 0) |
| Wear leveling, bad block | Bộ điều khiển trong thiết bị lo | Filesystem/kernel phải lo |
| Ví dụ | `/dev/sda`, `/dev/mmcblk0` | `/dev/mtd1` |
| Filesystem | ext4, FAT | JFFS2, UBIFS |

**Khi nào bắt buộc dùng MTD:** khi thiết bị có chip NAND/NOR flash hàn thẳng lên board, không
có controller (router, camera, thiết bị IoT giá rẻ). Khi đó phải dùng filesystem hiểu flash
như JFFS2/UBIFS – ext4 trên flash thô sẽ làm mòn hỏng nhanh các block bị ghi nhiều.

### G2. Vì sao mount JFFS2 qua `/dev/mtdblock1` nếu MTD không phải block device?

**Ngắn:** Lệnh `mount` cần một block device; `mtdblock` là lớp giả lập mỏng phía trên MTD để
`mount` hài lòng. Bên dưới, JFFS2 vẫn thao tác trực tiếp với MTD.

### G3. Hexdump đầu flash có `85 19` là gì?

**Ngắn:** Magic `0x1985` của JFFS2 (ghi little-endian). Mỗi node đều bắt đầu bằng magic này;
`01 e0` = node DIRENT (tên file), `02 e0` = node INODE (dữ liệu). JFFS2 là log-structured:
không có bảng cố định, khi mount phải quét toàn bộ flash.

---

## H. ioctl

### H1. Vì sao cần ioctl khi đã có read/write?

**Ngắn:** read/write dùng cho **dữ liệu**; ioctl dùng cho **điều khiển** (đổi chế độ, reset,
lấy thống kê) – những lệnh không phải là "dòng byte".

### H2. Magic number để làm gì?

**Ngắn:** 8 bit nhận dạng driver nhúng trong mã lệnh. Lệnh của driver khác gửi nhầm tới sẽ không
khớp → `-ENOTTY`, thay vì vô tình thực thi.

### H3. Tham số thứ ba của `_IOR(magic, nr, type)` để làm gì?

**Ngắn:** Macro lấy `sizeof(type)` mã hoá vào 14 bit SIZE của mã lệnh. Ví dụ
`ASGN1_GET_STATS = 0x8014a501` → DIR=2 (read), SIZE=0x14=20 byte = `sizeof(struct asgn1_stats)`,
TYPE=0xA5, NR=1.

### H4. Vì sao trả `-ENOTTY` cho lệnh lạ?

**Ngắn:** Quy ước POSIX "ioctl không phù hợp với thiết bị này"; userspace `perror` in ra
`Inappropriate ioctl for device`.

---

**Đọc tiếp:** quay lại [README](../../README.vi.md) hoặc xem [ASSIGNMENT-01](../../ASSIGNMENT1/README.vi.md).
