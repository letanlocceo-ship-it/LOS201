/*
 * File: circular_buffer.h
 * Purpose: Thread-safe circular buffer definitions
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t timestamp_ms;
    float temperature;
    float humidity;
} SensorData;

typedef struct {
    SensorData *buffer;
    int capacity;
    int head;
    int tail;
    int current_size;
    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;
    bool shutdown;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} CircularBuffer;

CircularBuffer* cb_init(int capacity);
void cb_destroy(CircularBuffer *cb);
int cb_push(CircularBuffer *cb, SensorData data);
int cb_pop(CircularBuffer *cb, SensorData *data);
void cb_shutdown(CircularBuffer *cb);

int cb_get_current_size(CircularBuffer *cb);
unsigned long cb_get_total_produced(CircularBuffer *cb);
unsigned long cb_get_total_consumed(CircularBuffer *cb);
unsigned long cb_get_overflow_count(CircularBuffer *cb);

#endif /* CIRCULAR_BUFFER_H */
