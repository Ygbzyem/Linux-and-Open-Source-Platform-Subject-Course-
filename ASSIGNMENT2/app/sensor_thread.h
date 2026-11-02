#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include "circular_buffer.h"

typedef struct {
    int interval_ms;
    int verbose;
    circular_buffer_t *buffer;
    const char *dev_path;
} sensor_config_t;

void* sensor_thread_func(void *arg);

#endif