# ASSIGNMENT-02: Sensor Monitoring System

> Bản tiếng Anh: [README.md](README.md)

## Overview
Hệ thống giám sát cảm biến Sensor Monitoring System (SMS) là ứng dụng nhúng đa luồng thời gian thực chạy trên môi trường mô phỏng ARMv7 QEMU Linux. Hệ thống bao gồm:
- **Kernel Driver (`sms_sensor.ko`)**: Mô phỏng cảm biến nhiệt độ và độ ẩm công nghiệp. Driver xuất dữ liệu qua character device `/dev/sms_sensor`, cung cấp thống kê qua `/proc/sms_stats`, và hỗ trợ cấu hình khoảng thời gian đọc tối thiểu bằng `ioctl` (`SMS_SET_INTERVAL`).
- **Ứng dụng Userspace (`sms_app`)**: Chương trình C đa luồng POSIX gồm 4 thread đồng bộ:
  - `Sensor Thread`: Đọc định kỳ dữ liệu CSV từ `/dev/sms_sensor` với định thời Real-time `SCHED_FIFO` và đẩy vào circular buffer.
  - `Processor Thread`: Lấy dữ liệu từ buffer, tính toán thống kê và phát hiện bất thường (nhiệt độ cao, độ ẩm cao, nhiệt độ thay đổi đột ngột).
  - `Alert Thread`: Lắng nghe cảnh báo từ POSIX message queue (`/sms_alarm_mq`) và in cảnh báo ra `stderr`.
  - `Logger Thread`: Nhận tin nhắn dữ liệu/cảnh báo từ `/sms_log_mq` và ghi bất đồng bộ ra file `/tmp/sms.log` hỗ trợ xoay vòng log (log rotation).

## Kiến trúc hệ thống
```
┌─────────────────────────────────────────────────────────────────────────┐
│ Kernel Space                                                            │
│  [sms_sensor.ko]                                                        │
│     ├── /dev/sms_sensor  ─── (read: dạng CSV, ioctl: SMS_SET_INTERVAL)    │
│     └── /proc/sms_stats ─── (read: uptime, read_count, giá trị gần nhất) │
└───────────────────────────────────┬─────────────────────────────────────┘
                                    │ IPC / System Calls
┌───────────────────────────────────▼─────────────────────────────────────┐
│ Ứng dụng Userspace (sms_app)                                            │
│  [Sensor Thread] ──(Circular Buffer)──> [Processor Thread]             │
│        (SCHED_FIFO)                           │                         │
│                                      (POSIX Message Queues)             │
│                                               ├─> [Alert Thread]        │
│                                               └─> [Logger Thread] ──> /tmp/sms.log
└─────────────────────────────────────────────────────────────────────────┘
```

## Cấu trúc thư mục
```
ASSIGNMENT2/
├── driver/
│   ├── sms_sensor.c       # Mã nguồn Kernel character device driver
│   ├── sms_sensor.h       # Header chung (định nghĩa ioctl, cấu trúc)
│   └── Makefile           # Makefile cross-compile cho Kernel module
├── app/
│   ├── circular_buffer.c/.h # Triển khai Circular Buffer an toàn đa luồng
│   ├── sensor_thread.c/.h   # Luồng đọc cảm biến (dùng SCHED_FIFO)
│   ├── processor_thread.c/.h# Luồng xử lý dữ liệu & phát hiện cảnh báo
│   ├── alert_thread.c/.h    # Luồng xuất cảnh báo ra stderr
│   ├── logger_thread.c/.h   # Luồng ghi log bất đồng bộ ra file
│   ├── sms_app.c            # File chính ứng dụng & xử lý tín hiệu SIGINT
│   └── Makefile             # Makefile cross-compile ứng dụng userspace
├── logs/                  # Thư mục lưu log và kết quả kiểm thử
├── Makefile               # Top-level Makefile biên dịch toàn bộ dự án
├── README.vi.md           # Tài liệu tiếng Việt
└── README.md              # Tài liệu hướng dẫn dự án
```

## Biên dịch (Build)
Biên dịch Cross-compile Kernel Module và File nhị phân tĩnh (Static Binary) cho kiến trúc ARMv7:

```bash
# Biên dịch driver và ứng dụng userspace
make -C driver
arm-linux-gnueabihf-gcc -Wall -Wextra -pthread -static -I../driver app/*.c -o app/sms_app -lrt -lm
```

## Chạy thử nghiệm (trên QEMU)
```bash
# 1. Nạp kernel module
insmod /lib/modules/5.15.221/sms_sensor.ko
dmesg | tail -n 5

# 2. Kiểm tra device node được tạo tự động
ls -l /dev/sms_sensor

# 3. Chạy ứng dụng giám sát (chu kỳ 500ms, chạy trong 10 giây, bật verbose mode)
sms_app -i 500 -t 40.0 -h 80.0 -d 10 -v

# 4. Kiểm tra file log tạo ra và thông số thống kê driver
cat /tmp/sms.log
cat /proc/sms_stats

# 5. Gỡ bỏ kernel module
rmmod sms_sensor
```

## Ví dụ kết quả thực thi
```text
/ # insmod /lib/modules/5.15.221/sms_sensor.ko
sms_sensor: driver loaded successfully
/ # ls -l /dev/sms_sensor
crw-------    1 root     0          248,   0 Nov  2 19:17 /dev/sms_sensor
/ # sms_app -i 500 -t 40.0 -h 80.0 -d 10 -v
[SENSOR] Time: 23788 ms | Temp: 41.92 C | Hum: 47.11 %
[ALERT][23788] TYPE: TEMP_HIGH VALUE: 41.92 THRESHOLD: 40.00
[SENSOR] Time: 24288 ms | Temp: 22.15 C | Hum: 65.40 %
...
SMS System shutdown complete.
/ # cat /proc/sms_stats
read_count: 20
last_temperature: 22.15
last_humidity: 65.40
driver_uptime_s: 45
```

## Các khái niệm cốt lõi (Key Concepts)
- **Character Device Driver**: Triển khai `file_operations` (`read`, `open`, `release`, `unlocked_ioctl`) và đăng ký Dynamic Major Number.
- **Tính toán số nguyên trong Kernel (Integer Math)**: Thay thế hoàn toàn phép tính số thực (`float`) bằng số nguyên nhân 100 (`temp_x100`) để tránh chi phí Context Switch FPU và lỗi liên kết `__aeabi` modpost trên ARM.
- **Giao diện procfs**: Xuất dữ liệu thống kê hệ thống thời gian thực tại `/proc/sms_stats` thông qua `seq_file`.
- **Đa luồng POSIX (`pthreads`)**: Chạy song song 4 luồng đảm nhận các nhiệm vụ riêng biệt.
- **Đồng bộ luồng (Thread Synchronization)**: Circular Buffer được bảo vệ bởi `pthread_mutex_t` và cơ chế báo hiệu `pthread_cond_t` (Producer-Consumer model).
- **POSIX Message Queues (`mqueue`)**: Cơ chế IPC truyền tin nhắn có cài đặt Timeout (`mq_timedsend`, `mq_timedreceive`) giữa luồng xử lý và luồng alert/logger nhằm tránh tình trạng treo luồng (Deadlock) khi queue bị đầy.
- **Lập lịch Thời gian thực (`SCHED_FIFO`)**: Ưu tiên lập lịch cho Sensor Thread kết hợp `clock_nanosleep(CLOCK_MONOTONIC)` giúp giảm thiểu độ lệch chu kỳ đọc (Jitter).
- **Dừng hệ thống an toàn (Graceful Shutdown)**: Bắt tín hiệu `sigaction(SIGINT)` giúp dọn dẹp Message Queue, ghi hết log buffer xuống đĩa và `pthread_join` các luồng sạch sẽ.

## Ghi chú & Hướng cải tiến (Notes)
- **Hạn chế hiện tại**:
  - Driver hiện tại tạo dữ liệu ngẫu nhiên bằng `get_random_u32()` thay vì kết nối với phần cứng cảm biến I2C/SPI thực tế.
  - Circular buffer sử dụng kích thước cố định (64 phần tử) cấp phát một lần trên Heap khi khởi động.
- **Hướng cải tiến trong tương lai**:
  - Tích hợp PREEMPT_RT kernel patch để giảm thêm jitter lập lịch xuống dưới 100µs.
  - Áp dụng kỹ thuật Memory Mapping (`mmap`) hoặc Ring-buffer động cho luồng dữ liệu cảm biến tần số cao.
  - Tích hợp ghi log bền vững lên bộ nhớ Flash sử dụng JFFS2/UBIFS.