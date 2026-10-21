/*
 * File: sensor_thread.c
 * Purpose: RT sensor reader
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#include "sensor_thread.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include <errno.h>

void* sensor_thread_func(void *arg) {
    AppContext *ctx = (AppContext*)arg;
    int fd = ctx->sensor_fd;

    if (ctx->sched_type == 1) {
        struct sched_param param = { .sched_priority = 50 };
        if (sched_setscheduler(0, SCHED_FIFO, &param) == 0) ctx->fifo_active = true;
        else { perror("[WARN] SCHED_FIFO failed"); ctx->fifo_active = false; }
    }

    char buf[64];
    struct timespec next_wake;
    clock_gettime(CLOCK_MONOTONIC, &next_wake);

    while (atomic_load(&ctx->sensor_running)) {
        long add_sec = ctx->interval_ms / 1000;
        long add_nsec = (ctx->interval_ms % 1000) * 1000000L;
        next_wake.tv_sec += add_sec;
        next_wake.tv_nsec += add_nsec;
        while (next_wake.tv_nsec >= 1000000000L) { next_wake.tv_sec++; next_wake.tv_nsec -= 1000000000L; }

        int sret;
        while ((sret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wake, NULL)) == EINTR) {
            if (!atomic_load(&ctx->sensor_running)) break;
        }
        if (!atomic_load(&ctx->sensor_running)) break;
        if (sret != 0) { perror("[ERROR] clock_nanosleep failed"); break; }

        struct timespec actual; clock_gettime(CLOCK_MONOTONIC, &actual);
        int64_t exp_us = (int64_t)next_wake.tv_sec * 1000000LL + next_wake.tv_nsec / 1000LL;
        int64_t act_us = (int64_t)actual.tv_sec * 1000000LL + actual.tv_nsec / 1000LL;
        int64_t jitter = act_us - exp_us;
        if (jitter < 0) jitter = -jitter;

        if (ctx->jitter_count < MAX_JITTER_SAMPLES) ctx->jitter_arr[ctx->jitter_count++] = jitter;

        ssize_t bytes = -1;
        int retry = 0;
        for (; retry < 10 && atomic_load(&ctx->sensor_running); retry++) {
            bytes = read(fd, buf, sizeof(buf) - 1);
            if (bytes > 0) break;
            if (bytes == 0) { lseek(fd, 0, SEEK_SET); struct timespec ts = {0, 1000000L}; nanosleep(&ts, NULL); }
            else if (errno == EAGAIN || errno == EINTR) { struct timespec ts = {0, 1000000L}; nanosleep(&ts, NULL); }
            else { perror("[ERROR] Sensor read err"); break; }
        }
        if (retry == 10) { ctx->sensor_dropped_reads++; continue; }

        if (bytes > 0) {
            buf[bytes] = '\0'; SensorData data; unsigned long long temp_ts = 0;
            if (sscanf(buf, "%llu,%f,%f", &temp_ts, &data.temperature, &data.humidity) == 3) {
                data.timestamp_ms = (uint64_t)temp_ts;
                (void)cb_push(ctx->cb, data);
            }
        }
    }
    return NULL;
}
