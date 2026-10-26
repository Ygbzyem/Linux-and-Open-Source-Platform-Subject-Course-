# 🐧 Embedded Linux – Sổ tay học tập

[English](README.md) | **Tiếng Việt**

> Nơi mình ghi lại quá trình học **Embedded Linux Systems** (FPT University) từ con số 0:
> build kernel, boot hệ thống ARM trên QEMU, viết kernel driver, và các assignment của môn.
>
> Repo này được viết để **đọc hiểu được** và **làm lại được từng bước**, kể cả khi không có internet.

**Tác giả:** Phạm Minh Tùng – SE204386

---

## 📌 Dùng repo này như thế nào?

| Bạn muốn… | Đọc ở đâu |
|---|---|
| Hiểu Embedded Linux là gì, hệ thống boot ra sao | [`docs/vi/01-embedded-linux-la-gi.md`](docs/vi/01-embedded-linux-la-gi.md) |
| Dựng lại toàn bộ môi trường trên một máy mới | [`docs/vi/02-cai-dat-moi-truong.md`](docs/vi/02-cai-dat-moi-truong.md) |
| **Đi thi: sửa driver → build → chạy → test nhanh nhất** | [`docs/vi/03-quy-trinh-thi.md`](docs/vi/03-quy-trinh-thi.md) ⭐ |
| Hiểu một character driver gồm những phần nào | [`docs/vi/04-giai-phau-driver.md`](docs/vi/04-giai-phau-driver.md) |
| Tra cứu API kernel (chữ ký hàm, include, ví dụ) | [`docs/vi/05-kernel-api.md`](docs/vi/05-kernel-api.md) |
| Gặp lỗi – tra theo triệu chứng | [`docs/vi/06-loi-thuong-gap.md`](docs/vi/06-loi-thuong-gap.md) |
| Ôn câu hỏi vấn đáp "tại sao…?" | [`docs/vi/07-van-dap.md`](docs/vi/07-van-dap.md) |

---

## 🧰 Môi trường

| Thành phần | Phiên bản / cấu hình |
|---|---|
| Máy host | Ubuntu 26.04 x86_64, 12 core |
| Cross-compiler | `arm-linux-gnueabihf-gcc` 15.2.0 |
| Board giả lập | QEMU `vexpress-a9` (ARM Cortex-A9, 512 MB RAM, 2 CPU) |
| Kernel target | **Linux 5.15.221** (`vexpress_defconfig`) |
| Bootloader | U-Boot 2022.04 |
| Root filesystem | BusyBox 1.35.0 (static) đóng gói thành initramfs |

> ⚠️ Không có board thật – mọi thứ chạy trên QEMU. Kernel của **máy host** (Ubuntu) và kernel
> của **máy target** (ARM trong QEMU) là hai kernel hoàn toàn khác nhau.

---

## 🗺️ Lộ trình học

```text
  LAB-01  Build hệ thống Embedded Linux tối thiểu
          toolchain → kernel → U-Boot → BusyBox → boot trên QEMU
             │
             ▼
  LAB-02  Viết character device driver
          file_operations → procfs/sysfs → MTD/JFFS2 → tự nạp khi boot
             │
             ▼
  ASSIGNMENT-01  Mở rộng driver
          Task 1: ioctl (4 lệnh) │ Task 2: misc device
             │
             ▼
  Bài luyện driver  (sửa driver LAB-02 theo yêu cầu – dạng đề thi)
             │
             ▼
  ASSIGNMENT-02  Sensor Monitoring System (capstone)
          kernel driver + app đa luồng + IPC + debug + real-time
```

---

## 📊 Tiến độ

| Phần | Nội dung | Trạng thái | Tài liệu |
|---|---|---|---|
| LAB-01 | Kernel, U-Boot, BusyBox, boot QEMU | ✅ Boot trực tiếp qua QEMU | [LAB1](LAB1/README.vi.md) |
| LAB-02 | Char driver, procfs/sysfs, MTD/JFFS2, auto-load | ✅ Phần A–D | [LAB2](LAB2/README.vi.md) |
| ASSIGNMENT-01 Task 1 | ioctl interface | ✅ 24/24 test PASS | [ASSIGNMENT1](ASSIGNMENT1/README.vi.md) |
| ASSIGNMENT-01 Task 2 | misc device `counter_driver` | ✅ 25/25 test PASS | [ASSIGNMENT1](ASSIGNMENT1/README.vi.md) |
| Bài luyện driver | Sửa driver theo đề | 📝 Kế hoạch | |
| ASSIGNMENT-02 | Sensor Monitoring System | 📝 Kế hoạch | |

---

## 📁 Cấu trúc repo

```text
.
├── README.md                ← bản tiếng Anh (README.vi.md: bạn đang ở đây)
├── docs/                    ← kiến thức + quy trình (đọc offline được); docs/vi/: tiếng Việt
├── tools/
│   └── dev.sh               ← build → chép vào rootfs → đóng gói → boot QEMU
├── LAB1/
│   ├── README.md
│   └── embedded_lab1/
│       ├── configs/         ← .config của kernel, U-Boot, BusyBox
│       ├── rootfs_custom/   ← file tự viết trong rootfs (rcS, inittab…)
│       ├── boot_log.txt
│       └── uboot_log.txt
├── LAB2/
│   ├── README.md
│   └── embedded_lab2/
│       ├── driver/          ← lab2_driver.c, Makefile, script test
│       ├── rootfs_custom/
│       └── logs/
└── ASSIGNMENT1/
    ├── README.md
    ├── task1_ioctl/         ← asgn1_driver.c, asgn1_ioctl.h, asgn1_app.c, test
    ├── task2_misc/
    └── logs/
```

**Những thứ cố ý KHÔNG có trong repo** (quá nặng hoặc build lại được): source kernel, source
U-Boot, source BusyBox, rootfs đã build, `zImage`, `*.ko`, `*.cpio.gz`. Cách tạo lại → xem
[`docs/vi/02-cai-dat-moi-truong.md`](docs/vi/02-cai-dat-moi-truong.md).

---

## ⚡ Chạy nhanh (khi môi trường đã có sẵn)

```bash
cd ASSIGNMENT1/task1_ioctl        # hoặc thư mục driver bất kỳ có Makefile
../../tools/dev.sh                # build + đóng gói + boot QEMU
```

Trong QEMU (dấu nhắc `/ # `):

```sh
insmod /lib/modules/5.15.221/asgn1_driver.ko
test_asgn1.sh
```

Thoát QEMU: **Ctrl+A rồi X**.

Đường dẫn cố định trên máy mình (các script và Makefile đang dựa vào chúng):

| Đường dẫn | Là gì |
|---|---|
| `~/embedded_lab1/kernel/linux-5.15.221` | Source kernel đã build (`KDIR` trong Makefile) |
| `~/embedded_lab1/output/` | `zImage`, `vexpress-v2p-ca9.dtb` |
| `~/embedded_lab2/rootfs/initramfs/` | Rootfs đang dùng |
| `~/embedded_lab2/output/initramfs_lab2.cpio.gz` | Rootfs đã đóng gói |

---

## 🔎 Tra cứu offline

```bash
# Tìm trong toàn bộ tài liệu
grep -rn --include='*.md' 'copy_to_user' .

# Xem chữ ký thật của một hàm kernel ngay trong source đã có trên máy
grep -rn 'int misc_register' ~/embedded_lab1/kernel/linux-5.15.221/include/

# Xem driver mẫu thật của kernel
ls ~/embedded_lab1/kernel/linux-5.15.221/drivers/misc/
```

---

## 📝 Quy ước commit

Repo dùng [Conventional Commits](https://www.conventionalcommits.org/):
`feat`, `fix`, `refactor`, `test`, `docs`, `build`, `chore`, `style` với scope
`lab1`, `lab2`, `asgn1`, `asgn2`, `tools`, `docs`, `repo`.

```text
feat(asgn1): add ioctl interface with 4 commands
fix(tools): handle filenames starting with dash in dev.sh
```

---

## 📄 License

[MIT](LICENSE). Đề bài (`*.docx`) thuộc về giảng viên môn học.
