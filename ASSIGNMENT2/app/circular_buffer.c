#include <stdlib.h>
#include "circular_buffer.h"

circular_buffer_t* cb_init(int capacity) {
    circular_buffer_t *cb = malloc(sizeof(circular_buffer_t));
    cb->data = malloc(sizeof(sensor_data_t) * capacity);
    cb->capacity = capacity;
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
    cb->total_produced = 0;
    cb->total_consumed = 0;
    cb->overflow_count = 0;
    pthread_mutex_init(&cb->lock, NULL);
    pthread_cond_init(&cb->not_empty, NULL);
    return cb;
}

void cb_free(circular_buffer_t *cb) {
    if (!cb) return;
    pthread_mutex_destroy(&cb->lock);
    pthread_cond_destroy(&cb->not_empty);
    free(cb->data);
    free(cb);
}

int cb_push(circular_buffer_t *cb, sensor_data_t item) {
    pthread_mutex_lock(&cb->lock);
    if (cb->count == cb->capacity) {
        cb->overflow_count++;
        pthread_mutex_unlock(&cb->lock);
        return -1; // Overflow
    }
    cb->data[cb->head] = item;
    cb->head = (cb->head + 1) % cb->capacity;
    cb->count++;
    cb->total_produced++;
    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
    return 0;
}

int cb_pop(circular_buffer_t *cb, sensor_data_t *item) {
    pthread_mutex_lock(&cb->lock);
    while (cb->count == 0) {
        pthread_cond_wait(&cb->not_empty, &cb->lock);
    }
    *item = cb->data[cb->tail];
    cb->tail = (cb->tail + 1) % cb->capacity;
    cb->count--;
    cb->total_consumed++;
    pthread_mutex_unlock(&cb->lock);
    return 0;
}