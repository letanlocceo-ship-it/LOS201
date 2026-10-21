#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#include <linux/ioctl.h>

/*
 * Shared constants between kernel driver and userspace test program.
 */
#define ASGN1_IOC_MAGIC      'A'
#define ASGN1_VERSION_LEN    64

/*
 * Statistics returned by ASGN1_GET_STATS.
 */
struct asgn1_stats {
    int open_count;
    int write_count;
    int read_count;
    int buffer_len;
    int last_write_size;
};

/*
 * ioctl command definitions
 *
 * _IO  : no data transfer
 * _IOR : kernel -> userspace
 * _IOW : userspace -> kernel
 */
#define ASGN1_RESET_BUFFER \
    _IO(ASGN1_IOC_MAGIC, 1)

#define ASGN1_GET_STATS \
    _IOR(ASGN1_IOC_MAGIC, 2, struct asgn1_stats)

#define ASGN1_SET_MODE \
    _IOW(ASGN1_IOC_MAGIC, 3, int)

#define ASGN1_GET_VERSION \
    _IOR(ASGN1_IOC_MAGIC, 4, char[ASGN1_VERSION_LEN])

#endif
