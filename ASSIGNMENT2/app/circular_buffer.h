#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>

typedef struct {
    unsigned long long timestamp_ms;
    float temperature;
    float humidity;
} sensor_data_t;

typedef struct {
    sensor_data_t *data;
    int capacity;
    int head;
    int tail;
    int count;
    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} circular_buffer_t;

circular_buffer_t* cb_init(int capacity);
void cb_free(circular_buffer_t *cb);
int cb_push(circular_buffer_t *cb, sensor_data_t item);
int cb_pop(circular_buffer_t *cb, sensor_data_t *item);

#endif