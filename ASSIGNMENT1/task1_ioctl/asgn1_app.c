/*
 * asgn1_app.c - chương trình userspace gọi ioctl tới /dev/asgn1
 * Cách dùng:  asgn1_app reset | stats | mode <0|1> | version | invalid
 * Tác giả: Pham Minh Tung - SE204386
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "asgn1_ioctl.h"

#define DEV_PATH "/dev/asgn1"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s reset | stats | mode <0|1> | version | invalid\n", prog);
}

int main(int argc, char *argv[])
{
    int fd, ret = 0;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open " DEV_PATH);
        return 1;
    }

    if (strcmp(argv[1], "reset") == 0) {
        if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) { perror("ioctl RESET_BUFFER"); ret = 1; }
        else printf("RESET_BUFFER: OK\n");

    } else if (strcmp(argv[1], "stats") == 0) {
        struct asgn1_stats st;
        if (ioctl(fd, ASGN1_GET_STATS, &st) < 0) { perror("ioctl GET_STATS"); ret = 1; }
        else {
            printf("GET_STATS:\n");
            printf("  open_count      = %d\n", st.open_count);
            printf("  write_count     = %d\n", st.write_count);
            printf("  read_count      = %d\n", st.read_count);
            printf("  buffer_len      = %d\n", st.buffer_len);
            printf("  last_write_size = %d\n", st.last_write_size);
        }

    } else if (strcmp(argv[1], "mode") == 0 && argc >= 3) {
        int m = atoi(argv[2]);
        if (ioctl(fd, ASGN1_SET_MODE, &m) < 0) { perror("ioctl SET_MODE"); ret = 1; }
        else printf("SET_MODE: mode=%d OK\n", m);

    } else if (strcmp(argv[1], "version") == 0) {
        char ver[ASGN1_VERSION_LEN];
        if (ioctl(fd, ASGN1_GET_VERSION, ver) < 0) { perror("ioctl GET_VERSION"); ret = 1; }
        else printf("GET_VERSION: %s\n", ver);

    } else if (strcmp(argv[1], "invalid") == 0) {
        /* gửi một lệnh KHÔNG tồn tại để kiểm tra driver trả -ENOTTY */
        if (ioctl(fd, _IO(ASGN1_MAGIC, 99)) < 0) { perror("ioctl INVALID (expected)"); ret = 1; }
        else printf("INVALID: unexpectedly succeeded!\n");

    } else {
        usage(argv[0]);
        ret = 1;
    }

    close(fd);
    return ret;
}
