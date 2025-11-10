#ifndef VDEV_H
#define VDEV_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#define MAX_VDEVS 16

typedef struct vdev vdev_t;

// 虚拟设备操作接口
typedef struct vdev_ops {
    int (*read)(void *dev, void *buf, unsigned int block_no);
    int (*write)(void *dev, const void *buf, unsigned int block_no);
    int (*sync)(void *dev);
} vdev_ops_t;

// 虚拟设备对象
struct vdev {
    char name[32];             // 设备名
    unsigned int dev_id;       // 唯一ID
    void *priv;                // 私有上下文 (disk结构或其他)
    vdev_ops_t ops;            // 操作函数表
};

// 设备表
typedef struct {
    vdev_t *devs[MAX_VDEVS];
    unsigned int count;
    pthread_mutex_t lock;
} vdev_table_t;

// 全局设备表
extern vdev_table_t vdev_table;

#endif