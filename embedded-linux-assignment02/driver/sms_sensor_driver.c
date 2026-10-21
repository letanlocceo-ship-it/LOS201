/*
 * File: sms_sensor_driver.c
 * Purpose: Kernel module simulating temperature and humidity sensor
 * Author: Le Tien Thanh - SE201799
 * Date: 2026-10-16
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/math64.h>
#include "sms_sensor.h"

#define DEVICE_NAME "sms_sensor"
#define PROC_NAME   "sms_stats"

static DEFINE_MUTEX(sms_lock);

static unsigned long read_count = 0;
static u32 last_temp_tenths = 250;
static u32 last_humid_tenths = 500;
static u32 min_interval_ms = SMS_DEFAULT_INTERVAL_MS;
static u64 last_read_ms = 0;
static u64 init_time_ms = 0;

static struct proc_dir_entry *sms_proc_entry;

static inline u64 ktime_get_ms(void)
{
    return (u64)ktime_to_ms(ktime_get());
}

static int sms_open(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static int sms_release(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static ssize_t sms_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char kbuf[64];
    int len;
    u64 now_ms;
    u32 rand_val1, rand_val2;
    u32 temp_t, humid_t;

    (void)file;
    (void)ppos;

    mutex_lock(&sms_lock);

    now_ms = ktime_get_ms();
    if (last_read_ms != 0 && (now_ms - last_read_ms) < min_interval_ms) {
        mutex_unlock(&sms_lock);
        return -EAGAIN;
    }

    rand_val1 = get_random_u32();
    rand_val2 = get_random_u32();

    temp_t = 150 + (rand_val1 % 301);
    humid_t = 300 + (rand_val2 % 601);

    last_read_ms = now_ms;
    last_temp_tenths = temp_t;
    last_humid_tenths = humid_t;
    read_count++;

    len = snprintf(kbuf, sizeof(kbuf), "%llu,%u.%u,%u.%u\n",
                   (unsigned long long)now_ms,
                   temp_t / 10, temp_t % 10,
                   humid_t / 10, humid_t % 10);

    mutex_unlock(&sms_lock);

    if (len < 0)
        return -EFAULT;
    if (count < (size_t)len)
        return -EINVAL;

    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    return len;
}

static long sms_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    __u32 new_interval;

    (void)file;

    if (_IOC_TYPE(cmd) != SMS_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case SMS_SET_INTERVAL:
        if (copy_from_user(&new_interval, (__u32 __user *)arg, sizeof(__u32)))
            return -EFAULT;
        if (new_interval == 0 || new_interval > 60000)
            return -EINVAL;
        mutex_lock(&sms_lock);
        min_interval_ms = new_interval;
        mutex_unlock(&sms_lock);
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .open           = sms_open,
    .release        = sms_release,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
};

static struct miscdevice sms_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &sms_fops,
    .mode  = 0666,
};

static int sms_proc_show(struct seq_file *m, void *v)
{
    u64 uptime_sec;
    unsigned long rc;
    u32 t_tenths, h_tenths, intv;

    (void)v;

    mutex_lock(&sms_lock);
    uptime_sec = (unsigned long)div_u64(ktime_get_ms() - init_time_ms, 1000);
    rc = read_count;
    t_tenths = last_temp_tenths;
    h_tenths = last_humid_tenths;
    intv = min_interval_ms;
    mutex_unlock(&sms_lock);

    seq_printf(m, "read_count: %lu\n", rc);
    seq_printf(m, "last_temperature: %u.%u C\n", t_tenths / 10, t_tenths % 10);
    seq_printf(m, "last_humidity: %u.%u %%\n", h_tenths / 10, h_tenths % 10);
    seq_printf(m, "uptime: %llu seconds\n", (unsigned long long)uptime_sec);
    seq_printf(m, "min_interval_ms: %u\n", intv);

    return 0;
}

static int sms_proc_open(struct inode *inode, struct file *file)
{
    (void)inode;
    return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
    .proc_open    = sms_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init sms_driver_init(void)
{
    int ret;

    init_time_ms = ktime_get_ms();

    ret = misc_register(&sms_misc_device);
    if (ret) {
        pr_err("sms_sensor_driver: failed to register misc device (%d)\n", ret);
        return ret;
    }

    sms_proc_entry = proc_create(PROC_NAME, 0444, NULL, &sms_proc_ops);
    if (!sms_proc_entry) {
        misc_deregister(&sms_misc_device);
        pr_err("sms_sensor_driver: failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    pr_info("sms_sensor_driver: initialized (/dev/%s, /proc/%s)\n",
            DEVICE_NAME, PROC_NAME);
    return 0;
}

static void __exit sms_driver_exit(void)
{
    proc_remove(sms_proc_entry);
    misc_deregister(&sms_misc_device);
    pr_info("sms_sensor_driver: unloaded (read_count=%lu)\n", read_count);
}

module_init(sms_driver_init);
module_exit(sms_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Le Tien Thanh - SE201799");
MODULE_DESCRIPTION("SMS Simulated Temperature & Humidity Sensor Driver");
MODULE_VERSION("1.0");