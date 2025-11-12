#ifndef DISK_H
#define DISK_H

#include "defs.h"
#include "vdev.h"

typedef struct disk {
    int fd;                  // 文件描述符（或者内存指针）
    vdev_t vdev;
    unsigned int total_blocks;
    unsigned int block_size;
} disk_t;


// Disk 操作函数
int disk_read(void *priv, void *buf, unsigned int block_no);
int disk_write(void *priv, const void *buf, unsigned int block_no);
int disk_sync(void *priv);

// 
int disk_init(disk_t *disk, const char *path, unsigned int block_size, unsigned int total_blocks);
#endif