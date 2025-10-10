#include "../include/disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

ssize_t disk_read(disk_t* dev, void* buf, size_t size, off_t offset) {
    if (!dev || !buf) 
        return -1;
    pthread_mutex_lock(&dev->lock);
    ssize_t n = pread(dev->fd, buf, size, offset);
    pthread_mutex_unlock(&dev->lock);

    return n;
}

ssize_t disk_write(disk_t* dev, const void* buf, size_t size, off_t offset) {
    if (!dev || !buf) 
        return -1;  
    pthread_mutex_lock(&dev->lock);
    ssize_t n = pwrite(dev->fd, buf, size, offset);
    pthread_mutex_unlock(&dev->lock);

    return n;
}

size_t disk_sync(disk_t *dev) {
    return 0;
}

diskrws_t disk_wrs = {
    .read = disk_read,
    .write = disk_write,
    .sync = disk_sync,
};

disk_t* disk_mount(const char* path, size_t bsize, size_t bcount) {
    if (!path || !bsize || !bcount) 
        return NULL;

    disk_t *d = malloc(sizeof(disk_t));
    if (!d)
        return NULL;

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open disk img file failed");
        free(d);
        return NULL;
    }

    d->fd = fd;
    strncpy(d->name, path, sizeof(d->name)-1);
    d->bsize = bsize;
    d->bcount = bcount;
    d->rws = &disk_wrs;
    pthread_mutex_init(&d->lock, NULL);

    return d;
}

void disk_umount(disk_t* d) {
    if (!d)
        return;
    
    close(d->fd);
    free(d);
}

void disk_init(disk_mangr_t* mangr) {
    memset(mangr, 0, sizeof(disk_mangr_t));
}

size_t disk_register(disk_mangr_t* mangr, disk_t* disk, diskrws_t* rws) {
    if (!mangr || !disk || !rws) 
        return -1;

    if (mangr->count >= MAX_DISKS) {
        return -1;
    }

    int id = mangr->count++;
    mangr->disks[id] = disk;
    mangr->ops[id] = rws;

    return id;
}

disk_t* disk_get(disk_mangr_t* mangr, int id) {
    if (id < 0 || id >= MAX_DISKS) {
        return NULL;
    }

    disk_t* disk = mangr->disks[id];

    return disk;
}