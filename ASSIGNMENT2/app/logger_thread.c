#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "logger_thread.h"

extern volatile int keep_running;

static void check_and_rotate(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && st.st_size > 1024 * 1024) { // 1MB
        char rotated[256];
        snprintf(rotated, sizeof(rotated), "%s.1", path);
        rename(path, rotated);
    }
}

void* logger_thread_func(void *arg) {
    logger_config_t *cfg = (logger_config_t*)arg;
    FILE *fp = fopen(cfg->log_path, "a");
    if (!fp) return NULL;

    char msg[256];
    int flush_cnt = 0;

    while (keep_running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        ssize_t bytes = mq_timedreceive(cfg->log_mq, msg, sizeof(msg), NULL, &ts);
        if (bytes > 0) {
            check_and_rotate(cfg->log_path);
            fputs(msg, fp);
        }

        if (++flush_cnt >= 5) {
            fflush(fp);
            flush_cnt = 0;
        }
    }

    fflush(fp);
    fclose(fp);
    return NULL;
}