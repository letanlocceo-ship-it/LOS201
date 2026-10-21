/*
 * File: circular_buffer.c
 * Purpose: Thread-safe circular buffer implementation
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#include "circular_buffer.h"
#include <stdlib.h>

CircularBuffer* cb_init(int capacity)
{
    if (capacity <= 0) return NULL;
    CircularBuffer *cb = malloc(sizeof(CircularBuffer));
    if (!cb) return NULL;
    cb->buffer = malloc(sizeof(SensorData) * (size_t)capacity);
    if (!cb->buffer) { free(cb); return NULL; }

    cb->capacity = capacity;
    cb->head = 0; cb->tail = 0; cb->current_size = 0;
    cb->total_produced = 0; cb->total_consumed = 0; cb->overflow_count = 0;
    cb->shutdown = false;

    pthread_mutexattr_t mattr;
    if (pthread_mutexattr_init(&mattr) != 0) { free(cb->buffer); free(cb); return NULL; }
    pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
    if (pthread_mutex_init(&cb->lock, &mattr) != 0) {
        pthread_mutexattr_destroy(&mattr); free(cb->buffer); free(cb); return NULL;
    }
    pthread_mutexattr_destroy(&mattr);

    if (pthread_cond_init(&cb->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&cb->lock); free(cb->buffer); free(cb); return NULL;
    }
    return cb;
}

void cb_destroy(CircularBuffer *cb) {
    if (!cb) return;
    pthread_mutex_destroy(&cb->lock);
    pthread_cond_destroy(&cb->not_empty);
    free(cb->buffer); free(cb);
}

int cb_push(CircularBuffer *cb, SensorData data) {
    pthread_mutex_lock(&cb->lock);
    if (cb->current_size == cb->capacity) {
        cb->overflow_count++;
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    cb->buffer[cb->head] = data;
    cb->head = (cb->head + 1) % cb->capacity;
    cb->current_size++; cb->total_produced++;
    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
    return 0;
}

int cb_pop(CircularBuffer *cb, SensorData *data) {
    pthread_mutex_lock(&cb->lock);
    while (cb->current_size == 0 && !cb->shutdown) {
        pthread_cond_wait(&cb->not_empty, &cb->lock);
    }
    if (cb->current_size == 0 && cb->shutdown) {
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    *data = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % cb->capacity;
    cb->current_size--; cb->total_consumed++;
    pthread_mutex_unlock(&cb->lock);
    return 0;
}

void cb_shutdown(CircularBuffer *cb) {
    pthread_mutex_lock(&cb->lock);
    cb->shutdown = true;
    pthread_cond_broadcast(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
}

int cb_get_current_size(CircularBuffer *cb) {
    pthread_mutex_lock(&cb->lock);
    int val = cb->current_size;
    pthread_mutex_unlock(&cb->lock);
    return val;
}

unsigned long cb_get_total_produced(CircularBuffer *cb) {
    pthread_mutex_lock(&cb->lock);
    unsigned long val = cb->total_produced;
    pthread_mutex_unlock(&cb->lock);
    return val;
}

unsigned long cb_get_total_consumed(CircularBuffer *cb) {
    pthread_mutex_lock(&cb->lock);
    unsigned long val = cb->total_consumed;
    pthread_mutex_unlock(&cb->lock);
    return val;
}

unsigned long cb_get_overflow_count(CircularBuffer *cb) {
    pthread_mutex_lock(&cb->lock);
    unsigned long val = cb->overflow_count;
    pthread_mutex_unlock(&cb->lock);
    return val;
}
