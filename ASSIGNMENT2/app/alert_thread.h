#ifndef ALERT_THREAD_H
#define ALERT_THREAD_H

#include <mqueue.h>

typedef struct {
    mqd_t alarm_mq;
    mqd_t log_mq;
} alert_config_t;

void* alert_thread_func(void *arg);

#endif