#ifndef PROCESSOR_THREAD_H
#define PROCESSOR_THREAD_H

#include <mqueue.h>
#include "circular_buffer.h"

typedef struct {
    char type[32];
    float value;
    float threshold;
    unsigned long long timestamp_ms;
} alarm_msg_t;

typedef struct {
    float temp_threshold;
    float humid_threshold;
    circular_buffer_t *buffer;
    mqd_t alarm_mq;
    mqd_t log_mq;
} processor_config_t;

void* processor_thread_func(void *arg);

#endif