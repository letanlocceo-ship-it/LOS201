/*
 * File: app_config.h
 * Purpose: Shared context and constants
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include <mqueue.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include "circular_buffer.h"

#define ALARM_QUEUE_NAME   "/sms_alarm_q"
#define LOG_QUEUE_NAME     "/sms_log_q"
#define DEFAULT_LOG_PATH   "/tmp/sms.log"
#define MSG_SIZE           256
#define MAX_JITTER_SAMPLES 5000

typedef struct {
    const char *device_path;
    const char *log_path;
    int sensor_fd;
    int interval_ms;
    int duration_sec;
    int sched_type;
    bool fifo_active;
    float temp_thresh;
    float humid_thresh;
    bool verbose;

    int64_t jitter_arr[MAX_JITTER_SAMPLES];
    int jitter_count;

    unsigned long alert_temp_high;
    unsigned long alert_humid_high;
    unsigned long alert_temp_spike;
    unsigned long sensor_dropped_reads;

    CircularBuffer *cb;
    mqd_t alarm_mq;
    mqd_t log_mq;

    atomic_bool sensor_running;
    atomic_bool alert_running;
    atomic_bool logger_running;
    atomic_int thread_error;
} AppContext;
#endif
