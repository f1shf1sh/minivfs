#include "disk.h"
#include "vdev.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


int disk_read(void *priv, void *buf, unsigned int block_no) {
    disk_t *d = (disk_t*)priv;
    int fd = d->fd;
    int count = d->block_size;
    int off = d->block_size*block_no;

    int r = pread(fd, buf, count, off);
    // pread failed
    if (r < 0) { 
        return -1;
    }
    return 0;
}

int disk_write(void *priv, const void *buf, unsigned int block_no) {
    disk_t *d = (disk_t*)priv;
    int fd = d->fd;
    int count = d->block_size;
    int off = d->block_size*block_no;

    int r = pwrite(fd, buf, count, off);
    if ( r < 0 ) {
        return -1;
    }
    return 0;
}

int disk_sync(void *priv) {
    disk_t *d = (disk_t*)priv;
    int fd = d->fd;

    return fsync(fd);
}

int disk_init(disk_t *d, const char *path, unsigned int block_size, unsigned int total_blocks) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    // init vdev_t
    d->vdev.ops.read = disk_read;
    d->vdev.ops.write = disk_write;
    d->vdev.ops.sync = disk_sync;
    d->vdev.dev_id = fd;
    d->vdev.priv = d;
    strncpy(d->vdev.name, path, 32);
    
    // init disk_t
    d->block_size = block_size;
    d->total_blocks = total_blocks;
    d->fd = fd;
    
    /*
    #TODO 增加磁盘大小判断
    */

    printf("[disk init] load img file %s\n", path);
    return 0;
}

void disk_destory(void *priv) {
    if (!priv)
        return;
    
    disk_t *disk = (disk_t*)priv;
    if (disk) {
        close(disk->fd);
    }

    free(disk);
}