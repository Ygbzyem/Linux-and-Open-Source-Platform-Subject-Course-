#ifndef LOGGER_THREAD_H
#define LOGGER_THREAD_H

#include <mqueue.h>

typedef struct {
    mqd_t log_mq;
    const char *log_path;
} logger_config_t;

void* logger_thread_func(void *arg);

#endif