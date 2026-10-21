/*
 * File: sms_app.c
 * Purpose: Main entry point
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "app_config.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"
#include "../driver/sms_sensor.h"

static atomic_bool g_stop = false;
static void sig_handler(int s) { (void)s; atomic_store(&g_stop, true); }
static int cmp_int64(const void *a, const void *b) { int64_t la=*(const int64_t*)a, lb=*(const int64_t*)b; return (la>lb)-(la<lb); }

int main(int argc, char *argv[]) {
    AppContext ctx = { .device_path = SMS_DEVICE_PATH, .log_path = DEFAULT_LOG_PATH, .sensor_fd = -1,
                       .interval_ms = 500, .duration_sec = 60, .sched_type = 0, .fifo_active = false,
                       .temp_thresh = 40.0f, .humid_thresh = 80.0f, .verbose = false, .jitter_count = 0,
                       .alert_temp_high = 0, .alert_humid_high = 0, .alert_temp_spike = 0, .sensor_dropped_reads = 0,
                       .sensor_running = true, .alert_running = true, .logger_running = true, .thread_error = 0 };
    int opt;
    while ((opt = getopt(argc, argv, "i:t:h:d:vs:p:l:")) != -1) {
        switch (opt) {
            case 'i': ctx.interval_ms = atoi(optarg); if(ctx.interval_ms<=0) return 1; break;
            case 't': ctx.temp_thresh = (float)atof(optarg); break;
            case 'h': ctx.humid_thresh = (float)atof(optarg); break;
            case 'd': ctx.duration_sec = atoi(optarg); break;
            case 'v': ctx.verbose = true; break;
            case 's': ctx.sched_type = atoi(optarg); break;
            case 'l': ctx.log_path = optarg; break;
        case 'p': ctx.device_path = optarg; break;
            default: return 1;
        }
    }

    mlockall(MCL_CURRENT | MCL_FUTURE);
    ctx.sensor_fd = open(ctx.device_path, O_RDONLY);
    if (ctx.sensor_fd < 0) { perror("[ERR] open device"); return 1; }

    __u32 interval = (__u32)ctx.interval_ms;
    if (ioctl(ctx.sensor_fd, SMS_SET_INTERVAL, &interval) < 0 && errno != ENOTTY) {
        perror("[ERR] ioctl"); close(ctx.sensor_fd); return 1;
    }

    struct sigaction sa; sa.sa_handler = sig_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);

    mq_unlink(ALARM_QUEUE_NAME); mq_unlink(LOG_QUEUE_NAME);
    struct mq_attr attr = { .mq_flags=0, .mq_maxmsg=10, .mq_msgsize=MSG_SIZE, .mq_curmsgs=0 };
    ctx.alarm_mq = mq_open(ALARM_QUEUE_NAME, O_CREAT|O_RDWR, 0666, &attr);
    ctx.log_mq = mq_open(LOG_QUEUE_NAME, O_CREAT|O_RDWR, 0666, &attr);
    ctx.cb = cb_init(64);

    if (ctx.alarm_mq == (mqd_t)-1 || ctx.log_mq == (mqd_t)-1 || !ctx.cb) return 1;

    pthread_t t_s, t_p, t_a, t_l;
    pthread_create(&t_l, NULL, logger_thread_func, &ctx);
    pthread_create(&t_a, NULL, alert_thread_func, &ctx);
    pthread_create(&t_p, NULL, processor_thread_func, &ctx);
    pthread_create(&t_s, NULL, sensor_thread_func, &ctx);

    printf("SMS App started (Intv: %dms, Dur: %ds)\n", ctx.interval_ms, ctx.duration_sec);
    int elapsed = 0;
    while (!atomic_load(&g_stop) && (ctx.duration_sec == 0 || elapsed < ctx.duration_sec)) { sleep(1); elapsed++; }

    // Staged Shutdown
    atomic_store(&ctx.sensor_running, false); pthread_join(t_s, NULL);
    cb_shutdown(ctx.cb); pthread_join(t_p, NULL);
    atomic_store(&ctx.alert_running, false); pthread_join(t_a, NULL);
    atomic_store(&ctx.logger_running, false); pthread_join(t_l, NULL);

    printf("\n--- ALERT SUMMARY ---\nTEMP_HIGH: %lu\nHUMID_HIGH: %lu\nTEMP_SPIKE: %lu\n", 
           ctx.alert_temp_high, ctx.alert_humid_high, ctx.alert_temp_spike);
    printf("\n--- BUFFER & SYSTEM SUMMARY ---\nTotal Produced: %lu\nTotal Consumed: %lu\nOverflows: %lu\nDropped: %lu\n",
           cb_get_total_produced(ctx.cb), cb_get_total_consumed(ctx.cb), cb_get_overflow_count(ctx.cb), ctx.sensor_dropped_reads);

    if (ctx.jitter_count > 0) {
        qsort(ctx.jitter_arr, ctx.jitter_count, sizeof(int64_t), cmp_int64);
        int64_t sum=0, min=ctx.jitter_arr[0], max=ctx.jitter_arr[ctx.jitter_count-1];
        int b1=0, b2=0, b3=0, b4=0;
        for (int i=0; i<ctx.jitter_count; i++) {
            sum += ctx.jitter_arr[i];
            if (ctx.jitter_arr[i]<=100) b1++; else if (ctx.jitter_arr[i]<=500) b2++; else if (ctx.jitter_arr[i]<=1000) b3++; else b4++;
        }
        int p99_idx = (int)((99LL * ctx.jitter_count + 99) / 100) - 1;
        int64_t p99 = ctx.jitter_arr[p99_idx < 0 ? 0 : p99_idx];
        printf("\n--- JITTER REPORT (%s) ---\nSamples: %d\nMin: %lld us\nAvg: %lld us\nMax: %lld us\nP99: %lld us\n",
               ctx.fifo_active ? "SCHED_FIFO" : "SCHED_OTHER", ctx.jitter_count, (long long)min, (long long)(sum/ctx.jitter_count), (long long)max, (long long)p99);
        printf("\n--- HISTOGRAM ---\n0-100us   : %d\n100-500us : %d\n500-1000us: %d\n>1000us   : %d\n", b1, b2, b3, b4);
    }
    
    close(ctx.sensor_fd); cb_destroy(ctx.cb); mq_close(ctx.alarm_mq); mq_close(ctx.log_mq);
    mq_unlink(ALARM_QUEUE_NAME); mq_unlink(LOG_QUEUE_NAME);
    return 0;
}
