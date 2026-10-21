/*
 * File: alert_thread.c
 * Purpose: Receive alerts and output to stderr
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
/*
 * File: alert_thread.c
 * Purpose: Receive alerts
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-17
 */
#include "alert_thread.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static void fwd_log(mqd_t mq, const char *msg) {
    struct timespec tm; clock_gettime(CLOCK_REALTIME, &tm);
    tm.tv_nsec += 100000000L; while(tm.tv_nsec>=1000000000L){tm.tv_sec++;tm.tv_nsec-=1000000000L;}
    (void)mq_timedsend(mq, msg, strlen(msg)+1, 0, &tm);
}
static void proc_msg(AppContext *ctx, const char *msg) {
    fprintf(stderr, "%s", msg); fwd_log(ctx->log_mq, msg);
    if (strstr(msg, "TEMP_HIGH")) ctx->alert_temp_high++;
    else if (strstr(msg, "HUMID_HIGH")) ctx->alert_humid_high++;
    else if (strstr(msg, "TEMP_SPIKE")) ctx->alert_temp_spike++;
}

void* alert_thread_func(void *arg) {
    AppContext *ctx = (AppContext*)arg; 
    char msg[MSG_SIZE + 1]; 
    unsigned int prio; 
    
    while (atomic_load(&ctx->alert_running)) {
        struct timespec tm; clock_gettime(CLOCK_REALTIME, &tm);
        tm.tv_nsec += 100000000L; while(tm.tv_nsec>=1000000000L){tm.tv_sec++;tm.tv_nsec-=1000000000L;}
        
        ssize_t n = mq_timedreceive(ctx->alarm_mq, msg, MSG_SIZE, &prio, &tm);
        if (n >= 0) { 
            msg[n] = '\0'; 
            proc_msg(ctx, msg); 
        }
        else if (errno != ETIMEDOUT && errno != EINTR) {
            perror("[ERROR] alert_thread mq_timedreceive failed");
            atomic_store(&ctx->thread_error, 1);
            break;
        }
    }
    
    struct timespec drain = {0, 0}; ssize_t n;
    while ((n = mq_timedreceive(ctx->alarm_mq, msg, MSG_SIZE, &prio, &drain)) >= 0) {
        msg[n] = '\0'; proc_msg(ctx, msg);
    }
    return NULL;
}
