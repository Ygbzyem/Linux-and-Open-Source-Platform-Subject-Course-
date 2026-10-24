/*
 * asgn1_ioctl.h - giao diện ioctl dùng chung giữa asgn1_driver (kernel)
 *                 và asgn1_app (userspace)
 * Tác giả: Pham Minh Tung - SE204386
 */
#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

/* Kernel và userspace lấy macro _IO/_IOR/_IOW từ hai header khác nhau */
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

/* Magic number: 8 bit "chữ ký" riêng của driver này */
#define ASGN1_MAGIC         0xA5

/* Kích thước buffer chứa chuỗi version ở userspace */
#define ASGN1_VERSION_LEN   64

/* Hai chế độ hoạt động cho ASGN1_SET_MODE */
#define ASGN1_MODE_KERNEL   0   /* thêm tiền tố "[KERNEL] " khi write */
#define ASGN1_MODE_ECHO     1   /* lưu nguyên bản (mặc định) */

/* Struct thống kê trả về qua ASGN1_GET_STATS */
struct asgn1_stats {
    int open_count;
    int write_count;
    int read_count;
    int buffer_len;
    int last_write_size;
};

/* Bốn lệnh ioctl: _IO(magic, nr) / _IOR / _IOW(magic, nr, kiểu dữ liệu) */
#define ASGN1_RESET_BUFFER  _IO (ASGN1_MAGIC, 0)
#define ASGN1_GET_STATS     _IOR(ASGN1_MAGIC, 1, struct asgn1_stats)
#define ASGN1_SET_MODE      _IOW(ASGN1_MAGIC, 2, int)
#define ASGN1_GET_VERSION   _IOR(ASGN1_MAGIC, 3, char[ASGN1_VERSION_LEN])

#endif /* ASGN1_IOCTL_H */



