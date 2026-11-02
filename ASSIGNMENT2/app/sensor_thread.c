#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include "sensor_thread.h"
#include "../driver/sms_sensor.h"

extern volatile int keep_running;

void* sensor_thread_func(void *arg) {
    sensor_config_t *cfg = (sensor_config_t*)arg;
    int fd = open(cfg->dev_path, O_RDONLY);
    if (fd < 0) {
        perror("Sensor Thread: open device failed");
        return NULL;
    }

    ioctl(fd, SMS_SET_INTERVAL, cfg->interval_ms);

    struct timespec next_wake;
    clock_gettime(CLOCK_MONOTONIC, &next_wake);

    while (keep_running) {
        next_wake.tv_nsec += cfg->interval_ms * 1000000L;
        if (next_wake.tv_nsec >= 1000000000L) {
            next_wake.tv_sec += 1;
            next_wake.tv_nsec -= 1000000000L;
        }

        char line[128];
        ssize_t bytes = read(fd, line, sizeof(line) - 1);
        if (bytes > 0) {
            line[bytes] = '\0';
            sensor_data_t data;
            if (sscanf(line, "%llu,%f,%f", &data.timestamp_ms, &data.temperature, &data.humidity) == 3) {
                if (cfg->verbose) {
                    printf("[SENSOR] Time: %llu ms | Temp: %.2f C | Hum: %.2f %%\n",
                           data.timestamp_ms, data.temperature, data.humidity);
                }
                if (cb_push(cfg->buffer, data) < 0) {
                    fprintf(stderr, "[WARNING] Shared buffer overflow!\n");
                }
            }
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wake, NULL);
    }

    close(fd);
    return NULL;
}