/*
 * File: logger_thread.c
 * Purpose: Persist logs to /tmp/sms.log with full timestamps, 1MB rotation, and 5s flush
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#include "logger_thread.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

static void w_rot(FILE **fp, const char *path, size_t *bw, const char *msg) {
    if (!*fp) return;
    
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char ts_buf[32];
    strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

    int w = fprintf(*fp, "[%s] %s", ts_buf, msg); 
    if (w > 0) *bw += (size_t)w;
    
    if (*bw > 1048576) {
        char old[256]; snprintf(old, sizeof(old), "%s.1", path);
        fflush(*fp); fclose(*fp); rename(path, old);
        *fp = fopen(path, "a"); if (*fp) *bw = 0;
    }
}

void* logger_thread_func(void *arg) {
    AppContext *ctx = (AppContext*)arg;
    FILE *fp = fopen(ctx->log_path, "a");
    if (!fp) { 
        perror("[ERROR] logger_thread failed to open log file");
        atomic_store(&ctx->thread_error, 1); 
        return NULL; 
    }

    struct stat st; size_t bw = (stat(ctx->log_path, &st)==0) ? st.st_size : 0;
    char msg[MSG_SIZE + 1]; unsigned int prio; time_t last_f = time(NULL); int err_cnt = 0;

    while (atomic_load(&ctx->logger_running)) {
        struct timespec tm; clock_gettime(CLOCK_REALTIME, &tm);
        tm.tv_nsec += 100000000L; while(tm.tv_nsec>=1000000000L){tm.tv_sec++;tm.tv_nsec-=1000000000L;}
        
        ssize_t n = mq_timedreceive(ctx->log_mq, msg, MSG_SIZE, &prio, &tm);
        if (n >= 0) { 
            msg[n] = '\0'; w_rot(&fp, ctx->log_path, &bw, msg); err_cnt = 0; 
        }
        else if (errno != ETIMEDOUT && errno != EINTR) {
            perror("[ERROR] logger_thread mq_timedreceive failed");
            if (++err_cnt > 5) { atomic_store(&ctx->thread_error, 1); break; }
            struct timespec wait_ts = {0, 10000000L}; nanosleep(&wait_ts, NULL);
        }
        if (fp && (time(NULL) - last_f >= 5)) { fflush(fp); last_f = time(NULL); }
    }
    
    struct timespec drain = {0, 0}; ssize_t n;
    while ((n = mq_timedreceive(ctx->log_mq, msg, MSG_SIZE, &prio, &drain)) >= 0) {
        msg[n] = '\0'; w_rot(&fp, ctx->log_path, &bw, msg);
    }
    if (fp) { fflush(fp); fclose(fp); }
    return NULL;
}
