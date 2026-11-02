#include <stdio.h>
#include <string.h>
#include <time.h>
#include "alert_thread.h"
#include "processor_thread.h"

extern volatile int keep_running;

void* alert_thread_func(void *arg) {
    alert_config_t *cfg = (alert_config_t*)arg;
    alarm_msg_t alarm;

    while (keep_running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        ssize_t bytes = mq_timedreceive(cfg->alarm_mq, (char*)&alarm, sizeof(alarm), NULL, &ts);
        if (bytes > 0) {
            fprintf(stderr, "[ALERT][%llu] TYPE: %s VALUE: %.2f THRESHOLD: %.2f\n",
                    alarm.timestamp_ms, alarm.type, alarm.value, alarm.threshold);

            char log_buf[256];
            snprintf(log_buf, sizeof(log_buf), "[ALERT][%llu] TYPE: %s VALUE: %.2f THRESHOLD: %.2f\n",
                     alarm.timestamp_ms, alarm.type, alarm.value, alarm.threshold);
            mq_send(cfg->log_mq, log_buf, strlen(log_buf) + 1, 0);
        }
    }
    return NULL;
}