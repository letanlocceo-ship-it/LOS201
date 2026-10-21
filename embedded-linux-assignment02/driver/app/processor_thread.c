/*
 * File: processor_thread.c
 * Purpose: Process data, moving average, alerts
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#include "processor_thread.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

static void safe_send(mqd_t mq, const char *msg) {
    struct timespec tm; clock_gettime(CLOCK_REALTIME, &tm);
    tm.tv_nsec += 100000000L;
    while (tm.tv_nsec >= 1000000000L) { tm.tv_sec++; tm.tv_nsec -= 1000000000L; }
    (void)mq_timedsend(mq, msg, strlen(msg) + 1, 0, &tm);
}

void* processor_thread_func(void *arg) {
    AppContext *ctx = (AppContext*)arg;
    SensorData data;
    float temp_hist[10]={0}, humid_hist[10]={0};
    int hist_idx=0, hist_count=0;
    bool has_prev = false; float prev_temp = 0;
    unsigned long seen_over = 0;

    while (cb_pop(ctx->cb, &data) == 0) {
        char msg[MSG_SIZE];
        unsigned long cur_over = cb_get_overflow_count(ctx->cb);
        if (cur_over > seen_over) {
            snprintf(msg, MSG_SIZE, "[WARN] [%llu] Buffer overflow\n", (unsigned long long)data.timestamp_ms);
            safe_send(ctx->log_mq, msg); seen_over = cur_over;
        }

        temp_hist[hist_idx] = data.temperature; humid_hist[hist_idx] = data.humidity;
        hist_idx = (hist_idx + 1) % 10; if (hist_count < 10) hist_count++;

        float t_avg = 0, h_avg = 0;
        for (int i=0; i<hist_count; i++) { t_avg += temp_hist[i]; h_avg += humid_hist[i]; }
        t_avg /= hist_count; h_avg /= hist_count;

        if (ctx->verbose) printf("Sample: TS=%llu T=%.1f(Avg=%.1f) H=%.1f(Avg=%.1f)\n", 
                                 (unsigned long long)data.timestamp_ms, data.temperature, t_avg, data.humidity, h_avg);

        if (data.temperature > ctx->temp_thresh) {
            snprintf(msg, MSG_SIZE, "[ALERT] [%llu] TYPE: TEMP_HIGH VALUE: %.1f THRESHOLD: %.1f\n", 
                     (unsigned long long)data.timestamp_ms, data.temperature, ctx->temp_thresh);
            safe_send(ctx->alarm_mq, msg);
        }
        if (data.humidity > ctx->humid_thresh) {
            snprintf(msg, MSG_SIZE, "[ALERT] [%llu] TYPE: HUMID_HIGH VALUE: %.1f THRESHOLD: %.1f\n", 
                     (unsigned long long)data.timestamp_ms, data.humidity, ctx->humid_thresh);
            safe_send(ctx->alarm_mq, msg);
        }
        if (has_prev && fabsf(data.temperature - prev_temp) > 5.0f) {
            snprintf(msg, MSG_SIZE, "[ALERT] [%llu] TYPE: TEMP_SPIKE VALUE: %.1f THRESHOLD: 5.0\n", 
                     (unsigned long long)data.timestamp_ms, data.temperature);
            safe_send(ctx->alarm_mq, msg);
        }
        prev_temp = data.temperature; has_prev = true;

        snprintf(msg, MSG_SIZE, "[INFO] [%llu] Temp=%.1f Hum=%.1f\n", 
                 (unsigned long long)data.timestamp_ms, data.temperature, data.humidity);
        safe_send(ctx->log_mq, msg);
    }
    return NULL;
}
