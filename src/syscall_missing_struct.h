#include <stdint.h>


/* Definitions from ubuntu linux manual page for statx */

struct statx_timestamp {
    int64_t tv_sec;    /* Seconds since the Epoch (UNIX time) */
    uint32_t tv_nsec;   /* Nanoseconds since tv_sec */
};

struct statx {
    uint32_t stx_mask;        /* Mask of bits indicating
                                filled fields */
    uint32_t stx_blksize;     /* Block size for filesystem I/O */
    uint64_t stx_attributes;  /* Extra file attribute indicators */
    uint32_t stx_nlink;       /* Number of hard links */
    uint32_t stx_uid;         /* User ID of owner */
    uint32_t stx_gid;         /* Group ID of owner */
    uint16_t stx_mode;        /* File type and mode */
    uint64_t stx_ino;         /* Inode number */
    uint64_t stx_size;        /* Total size in bytes */
    uint64_t stx_blocks;      /* Number of 512B blocks allocated */
    uint64_t stx_attributes_mask;
                            /* Mask to show what's supported
                                in stx_attributes */

    /* The following fields are file timestamps */
    struct statx_timestamp stx_atime;  /* Last access */
    struct statx_timestamp stx_btime;  /* Creation */
    struct statx_timestamp stx_ctime;  /* Last status change */
    struct statx_timestamp stx_mtime;  /* Last modification */

    /* If this file represents a device, then the next two
        fields contain the ID of the device */
    uint32_t stx_rdev_major;  /* Major ID */
    uint32_t stx_rdev_minor;  /* Minor ID */

    /* The next two fields contain the ID of the device
        containing the filesystem where the file resides */
    uint32_t stx_dev_major;   /* Major ID */
    uint32_t stx_dev_minor;   /* Minor ID */
};


//////////////////////////////////////////////////////////////////////

/* Definitions from ubuntu linux manual page for sched_setattr */


struct sched_attr {
    uint32_t size;              /* Size of this structure */
    uint32_t sched_policy;      /* Policy (SCHED_*) */
    uint64_t sched_flags;       /* Flags */
    int32_t sched_nice;        /* Nice value (SCHED_OTHER,
                                SCHED_BATCH) */
    uint32_t sched_priority;    /* Static priority (SCHED_FIFO,
                                SCHED_RR) */
    /* Remaining fields are for SCHED_DEADLINE */
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};