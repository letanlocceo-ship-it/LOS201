#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/timekeeping.h>
#include <linux/ioctl.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02 Practice Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME     "lab2"
#define CLASS_NAME      "lab2_class"
#define BUFFER_SIZE     1024
#define DRIVER_MAJOR    240
#define DRIVER_VERSION  "1.0"

/* ioctl commands */
#define LAB2_IOC_MAGIC       'L'
#define GET_BUFFER_LEN       _IOR(LAB2_IOC_MAGIC, 1, int)
#define CLEAR_BUFFER         _IO(LAB2_IOC_MAGIC, 2)

/* =========================================================
 * Global data
 * ========================================================= */
static int major_number = DRIVER_MAJOR;

static char device_buffer[BUFFER_SIZE];
static int buffer_len;

/* Default maximum bytes accepted by one write() */
static unsigned int max_size = 128;

/* Statistics */
static unsigned long long open_count;
static unsigned long long read_count;
static unsigned long long write_count;

static unsigned long long total_bytes_read;
static unsigned long long total_bytes_written;

static size_t last_write_size;
static time64_t last_write_time;

/*
 * Read-once state:
 * true  = current data can still be read
 * false = data was completely consumed, wait for new write()
 */
static bool data_available;

static struct class *lab2_class;
static struct device *lab2_device;

static DEFINE_MUTEX(lab2_mutex);

/* =========================================================
 * Helper
 * ========================================================= */

static void lab2_clear_buffer_locked(void)
{
    memset(device_buffer, 0, BUFFER_SIZE);
    buffer_len = 0;
    data_available = false;
}

/* =========================================================
 * Character Device Operations
 * ========================================================= */

static int lab2_open(struct inode *inode, struct file *file)
{
    unsigned long long count;

    mutex_lock(&lab2_mutex);

    open_count++;
    count = open_count;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: device opened (count=%llu)\n", count);

    return 0;
}

static int lab2_release(struct inode *inode, struct file *file)
{
    pr_info("lab2_driver: device closed\n");

    return 0;
}

/*
 * read()
 *
 * - protected by mutex
 * - increments read_count for every read() call
 * - accumulates total_bytes_read
 * - implements global read-once behavior
 */
static ssize_t lab2_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *offset)
{
    ssize_t bytes_read;

    mutex_lock(&lab2_mutex);

    read_count++;

    /*
     * No new unread data.
     */
    if (!data_available) {
        mutex_unlock(&lab2_mutex);
        return 0;
    }

    if (*offset >= buffer_len) {
        data_available = false;

        mutex_unlock(&lab2_mutex);
        return 0;
    }

    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&lab2_mutex);

        pr_err("lab2_driver: copy_to_user failed\n");
        return -EFAULT;
    }

    *offset += len;

    bytes_read = len;

    total_bytes_read += bytes_read;

    /*
     * Entire current buffer has been consumed.
     * No more read until a new write().
     */
    if (*offset >= buffer_len)
        data_available = false;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: sent %zd bytes to user\n", bytes_read);

    return bytes_read;
}

/*
 * write()
 *
 * Features:
 * - default max 128 bytes per write
 * - configurable max_size through sysfs
 * - append instead of overwrite
 * - "clear" and "clear\n" clear the buffer
 * - updates statistics and timestamp
 */
static ssize_t lab2_write(struct file *file,
                          const char __user *buf,
                          size_t len,
                          loff_t *offset)
{
    char temp_buffer[BUFFER_SIZE];

    size_t write_len;
    size_t remaining;

    unsigned int current_max;

    if (len == 0)
        return 0;

    /*
     * Take a snapshot of current max_size.
     */
    mutex_lock(&lab2_mutex);
    current_max = max_size;
    mutex_unlock(&lab2_mutex);

    write_len = len;

    /*
     * Requirement:
     * maximum write size defaults to 128 bytes.
     */
    if (write_len > current_max) {
        pr_warn("lab2_driver: write truncated from %zu to %u bytes\n",
                write_len, current_max);

        write_len = current_max;
    }

    if (write_len > BUFFER_SIZE - 1)
        write_len = BUFFER_SIZE - 1;

    /*
     * First copy user data into a temporary kernel buffer.
     */
    if (copy_from_user(temp_buffer, buf, write_len)) {
        pr_err("lab2_driver: copy_from_user failed\n");
        return -EFAULT;
    }

    temp_buffer[write_len] = '\0';

    /*
     * Commands:
     *
     * echo -n clear > /dev/lab2
     * echo clear    > /dev/lab2
     */
    if ((write_len == 5 &&
         memcmp(temp_buffer, "clear", 5) == 0) ||
        (write_len == 6 &&
         memcmp(temp_buffer, "clear\n", 6) == 0)) {

        mutex_lock(&lab2_mutex);

        lab2_clear_buffer_locked();

        write_count++;
        total_bytes_written += write_len;

        last_write_size = write_len;
        last_write_time = ktime_get_real_seconds();

        mutex_unlock(&lab2_mutex);

        pr_info("lab2_driver: buffer cleared by write command\n");

        return write_len;
    }

    mutex_lock(&lab2_mutex);

    /*
     * Append mode.
     */
    remaining = (BUFFER_SIZE - 1) - buffer_len;

    if (remaining == 0) {
        mutex_unlock(&lab2_mutex);

        pr_warn("lab2_driver: buffer full\n");
        return -ENOSPC;
    }

    if (write_len > remaining) {
        pr_warn("lab2_driver: append truncated to %zu bytes\n",
                remaining);

        write_len = remaining;
    }

    memcpy(device_buffer + buffer_len,
           temp_buffer,
           write_len);

    buffer_len += write_len;

    device_buffer[buffer_len] = '\0';

    /*
     * New data becomes readable again.
     */
    data_available = true;

    write_count++;

    total_bytes_written += write_len;

    last_write_size = write_len;
    last_write_time = ktime_get_real_seconds();

    pr_info("lab2_driver: received %zu bytes, buffer_len=%d\n",
            write_len, buffer_len);

    mutex_unlock(&lab2_mutex);

    return write_len;
}

/* =========================================================
 * IOCTL
 * ========================================================= */

static long lab2_ioctl(struct file *file,
                       unsigned int cmd,
                       unsigned long arg)
{
    int len;

    switch (cmd) {

    case GET_BUFFER_LEN:

        mutex_lock(&lab2_mutex);

        len = buffer_len;

        mutex_unlock(&lab2_mutex);

        if (copy_to_user((int __user *)arg,
                         &len,
                         sizeof(len)))
            return -EFAULT;

        return 0;

    case CLEAR_BUFFER:

        mutex_lock(&lab2_mutex);

        lab2_clear_buffer_locked();

        mutex_unlock(&lab2_mutex);

        pr_info("lab2_driver: buffer cleared through ioctl\n");

        return 0;

    default:

        return -ENOTTY;
    }
}

/* =========================================================
 * file_operations
 * ========================================================= */

static const struct file_operations lab2_fops = {
    .owner          = THIS_MODULE,
    .open           = lab2_open,
    .release        = lab2_release,
    .read           = lab2_read,
    .write          = lab2_write,
    .unlocked_ioctl = lab2_ioctl,
};

/* =========================================================
 * PROCFS
 * ========================================================= */

static struct proc_dir_entry *proc_entry;

static int lab2_proc_show(struct seq_file *m, void *v)
{
    mutex_lock(&lab2_mutex);

    seq_printf(m,
               "LAB-02 Driver Statistics\n");

    seq_printf(m,
               "========================\n");

    seq_printf(m,
               "Driver name         : %s\n",
               DEVICE_NAME);

    seq_printf(m,
               "Driver version      : %s\n",
               DRIVER_VERSION);

    seq_printf(m,
               "Major number        : %d\n",
               major_number);

    seq_printf(m,
               "Buffer size         : %d bytes\n",
               BUFFER_SIZE);

    seq_printf(m,
               "Max write size      : %u bytes\n",
               max_size);

    seq_printf(m,
               "Data length         : %d bytes\n",
               buffer_len);

    seq_printf(m,
               "Status              : %s\n",
               buffer_len == 0 ? "EMPTY" : "DATA");

    seq_printf(m,
               "Open count          : %llu\n",
               open_count);

    seq_printf(m,
               "Read count          : %llu\n",
               read_count);

    seq_printf(m,
               "Write count         : %llu\n",
               write_count);

    seq_printf(m,
               "Total bytes read    : %llu\n",
               total_bytes_read);

    seq_printf(m,
               "Total bytes written : %llu\n",
               total_bytes_written);

    seq_printf(m,
               "Last write size     : %zu bytes\n",
               last_write_size);

    seq_printf(m,
               "Last write time     : %lld (Unix epoch)\n",
               (long long)last_write_time);

    seq_printf(m,
               "Read available      : %s\n",
               data_available ? "YES" : "NO");

    if (buffer_len > 0)
        seq_printf(m,
                   "Last data           : [%s]\n",
                   device_buffer);

    mutex_unlock(&lab2_mutex);

    return 0;
}

static int lab2_proc_open(struct inode *inode,
                          struct file *file)
{
    return single_open(file,
                       lab2_proc_show,
                       NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* =========================================================
 * SYSFS
 * ========================================================= */

/* buffer_len */

static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    int value;

    mutex_lock(&lab2_mutex);

    value = buffer_len;

    mutex_unlock(&lab2_mutex);

    return sprintf(buf, "%d\n", value);
}

static DEVICE_ATTR_RO(buffer_len);

/* buffer_size */

static ssize_t buffer_size_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    return sprintf(buf, "%d\n", BUFFER_SIZE);
}

static DEVICE_ATTR_RO(buffer_size);

/* open_count */

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    unsigned long long value;

    mutex_lock(&lab2_mutex);

    value = open_count;

    mutex_unlock(&lab2_mutex);

    return sprintf(buf, "%llu\n", value);
}

static DEVICE_ATTR_RO(open_count);

/* read_count */

static ssize_t read_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    unsigned long long value;

    mutex_lock(&lab2_mutex);

    value = read_count;

    mutex_unlock(&lab2_mutex);

    return sprintf(buf, "%llu\n", value);
}

static DEVICE_ATTR_RO(read_count);

/* last_data */

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);

    if (buffer_len > 0)
        ret = sprintf(buf,
                      "%s\n",
                      device_buffer);
    else
        ret = sprintf(buf,
                      "(empty)\n");

    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(last_data);

/* status */

static ssize_t status_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf)
{
    bool empty;

    mutex_lock(&lab2_mutex);

    empty = (buffer_len == 0);

    mutex_unlock(&lab2_mutex);

    return sprintf(buf,
                   "%s\n",
                   empty ? "EMPTY" : "DATA");
}

static DEVICE_ATTR_RO(status);

/* version */

static ssize_t version_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    return sprintf(buf,
                   "%s\n",
                   DRIVER_VERSION);
}

static DEVICE_ATTR_RO(version);

/* max_size: read/write */

static ssize_t max_size_show(struct device *dev,
                             struct device_attribute *attr,
                             char *buf)
{
    unsigned int value;

    mutex_lock(&lab2_mutex);

    value = max_size;

    mutex_unlock(&lab2_mutex);

    return sprintf(buf,
                   "%u\n",
                   value);
}

static ssize_t max_size_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf,
                              size_t count)
{
    unsigned int value;
    int ret;

    ret = kstrtouint(buf, 0, &value);

    if (ret)
        return ret;

    if (value < 1 ||
        value > BUFFER_SIZE - 1)
        return -EINVAL;

    mutex_lock(&lab2_mutex);

    max_size = value;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: max_size changed to %u\n",
            value);

    return count;
}

static DEVICE_ATTR_RW(max_size);

/* last_write_size */

static ssize_t last_write_size_show(
                    struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    size_t value;

    mutex_lock(&lab2_mutex);

    value = last_write_size;

    mutex_unlock(&lab2_mutex);

    return sprintf(buf,
                   "%zu\n",
                   value);
}

static DEVICE_ATTR_RO(last_write_size);

/* reset statistics */

static ssize_t reset_stats_store(
                    struct device *dev,
                    struct device_attribute *attr,
                    const char *buf,
                    size_t count)
{
    if (!sysfs_streq(buf, "reset"))
        return -EINVAL;

    mutex_lock(&lab2_mutex);

    open_count = 0;
    read_count = 0;
    write_count = 0;

    total_bytes_read = 0;
    total_bytes_written = 0;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: statistics reset\n");

    return count;
}

static DEVICE_ATTR_WO(reset_stats);

/* =========================================================
 * SYSFS attribute group
 * ========================================================= */

static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_buffer_size.attr,
    &dev_attr_open_count.attr,
    &dev_attr_read_count.attr,
    &dev_attr_last_data.attr,
    &dev_attr_status.attr,
    &dev_attr_version.attr,
    &dev_attr_max_size.attr,
    &dev_attr_last_write_size.attr,
    &dev_attr_reset_stats.attr,
    NULL,
};

static const struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};

/* =========================================================
 * Module init
 * ========================================================= */

static int __init lab2_init(void)
{
    int ret;

    pr_info("lab2_driver: initializing practice module\n");

    ret = register_chrdev(major_number,
                          DEVICE_NAME,
                          &lab2_fops);

    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed: %d\n",
               ret);
        return ret;
    }

    lab2_class = class_create(THIS_MODULE,
                              CLASS_NAME);

    if (IS_ERR(lab2_class)) {
        ret = PTR_ERR(lab2_class);

        unregister_chrdev(major_number,
                          DEVICE_NAME);

        return ret;
    }

    lab2_device = device_create(
                        lab2_class,
                        NULL,
                        MKDEV(major_number, 0),
                        NULL,
                        DEVICE_NAME);

    if (IS_ERR(lab2_device)) {
        ret = PTR_ERR(lab2_device);

        class_destroy(lab2_class);

        unregister_chrdev(major_number,
                          DEVICE_NAME);

        return ret;
    }

    ret = sysfs_create_group(
                &lab2_device->kobj,
                &lab2_attr_group);

    if (ret) {
        pr_err("lab2_driver: sysfs_create_group failed\n");

        device_destroy(lab2_class,
                       MKDEV(major_number, 0));

        class_destroy(lab2_class);

        unregister_chrdev(major_number,
                          DEVICE_NAME);

        return ret;
    }

    proc_entry = proc_create(
                    "lab2_info",
                    0444,
                    NULL,
                    &lab2_proc_fops);

    if (!proc_entry) {
        pr_err("lab2_driver: failed to create /proc/lab2_info\n");

        sysfs_remove_group(
                &lab2_device->kobj,
                &lab2_attr_group);

        device_destroy(lab2_class,
                       MKDEV(major_number, 0));

        class_destroy(lab2_class);

        unregister_chrdev(major_number,
                          DEVICE_NAME);

        return -ENOMEM;
    }

    pr_info("lab2_driver: /proc/lab2_info created\n");

    pr_info("lab2_driver: loaded, major=%d\n",
            major_number);

    return 0;
}

/* =========================================================
 * Module exit
 * ========================================================= */

static void __exit lab2_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);

    sysfs_remove_group(
            &lab2_device->kobj,
            &lab2_attr_group);

    device_destroy(
            lab2_class,
            MKDEV(major_number, 0));

    class_destroy(lab2_class);

    unregister_chrdev(
            major_number,
            DEVICE_NAME);

    pr_info("lab2_driver: practice module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
