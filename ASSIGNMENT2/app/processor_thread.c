#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <mqueue.h>
#include "processor_thread.h"
#include "circular_buffer.h"

extern volatile int keep_running;

void* processor_thread_func(void *arg) {
    processor_config_t *config = (processor_config_t*)arg;
    float prev_temp = -999.0f;

    while (keep_running) {
        sensor_data_t sample;
        if (cb_pop(config->buffer, &sample) != 0) continue;

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000000L; // Timeout 100ms
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }

        // Alert logic
        if (sample.temperature > config->temp_threshold) {
            alarm_msg_t alarm;
            strncpy(alarm.type, "TEMP_HIGH", sizeof(alarm.type));
            alarm.value = sample.temperature;
            alarm.threshold = config->temp_threshold;
            alarm.timestamp_ms = sample.timestamp_ms;
            mq_timedsend(config->alarm_mq, (const char*)&alarm, sizeof(alarm), 0, &ts);
        }
        if (sample.humidity > config->humid_threshold) {
            alarm_msg_t alarm;
            strncpy(alarm.type, "HUMID_HIGH", sizeof(alarm.type));
            alarm.value = sample.humidity;
            alarm.threshold = config->humid_threshold;
            alarm.timestamp_ms = sample.timestamp_ms;
            mq_timedsend(config->alarm_mq, (const char*)&alarm, sizeof(alarm), 0, &ts);
        }
        if (prev_temp != -999.0f && fabsf(sample.temperature - prev_temp) > 5.0f) {
            alarm_msg_t alarm;
            strncpy(alarm.type, "TEMP_SPIKE", sizeof(alarm.type));
            alarm.value = sample.temperature;
            alarm.threshold = 5.0f;
            alarm.timestamp_ms = sample.timestamp_ms;
            mq_timedsend(config->alarm_mq, (const char*)&alarm, sizeof(alarm), 0, &ts);
        }
        prev_temp = sample.temperature;

        // Log sample message
        char log_str[256];
        snprintf(log_str, sizeof(log_str), "[DATA][%llu] Temp: %.2f, Hum: %.2f\n",
                 sample.timestamp_ms, sample.temperature, sample.humidity);
        mq_timedsend(config->log_mq, log_str, strlen(log_str) + 1, 0, &ts);
    }
    return NULL;
}