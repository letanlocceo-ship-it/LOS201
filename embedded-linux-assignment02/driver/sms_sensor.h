/*
 * File: sms_sensor.h
 * Purpose: Header for SMS sensor driver ioctl definitions and constants
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-16
 */

#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SMS_DEVICE_PATH          "/dev/sms_sensor"
#define SMS_PROC_PATH            "/proc/sms_stats"
#define SMS_DEFAULT_INTERVAL_MS  100

#define SMS_IOC_MAGIC            'x'
#define SMS_SET_INTERVAL         _IOW(SMS_IOC_MAGIC, 1, __u32)

#endif /* SMS_SENSOR_H */
