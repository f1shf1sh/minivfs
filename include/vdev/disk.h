#ifndef DISK_H
#define DISK_H

#include "defs.h"
#include "vdev.h"

typedef struct disk {
    int fd;
    vdev_t vdev;
    unsigned int total_blocks;
    unsigned int block_size;
} disk_t;


// Disk 操作函数
int disk_read(void *priv, void *buf, unsigned int block_no);
int disk_write(void *priv, const void *buf, unsigned int block_no);
int disk_sync(void *priv);

/* A zero total_blocks derives the capacity from the image file. */
int disk_init(disk_t *disk, const char *path, unsigned int block_size, unsigned int total_blocks);
/* Close and release a heap-allocated disk owned by the caller. */
void disk_destroy(disk_t *disk);
#endif
