#include "../include/disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

disk_mgr_t disk_mgr;

ssize_t disk_read(struct disk* dev, void* buf, size_t size, off_t offset) {
    if (!dev || !buf) 
        return -1;
    pthread_mutex_lock(&dev->lock);
    ssize_t n = pread(dev->fd, buf, size, offset);
    pthread_mutex_unlock(&dev->lock);

    return n;
}

ssize_t disk_write(struct disk* dev, const void* buf, size_t size, off_t offset) {
    if (!dev || !buf) 
        return -1;  
    pthread_mutex_lock(&dev->lock);
    ssize_t n = pwrite(dev->fd, buf, size, offset);
    pthread_mutex_unlock(&dev->lock);

    return n;
}

size_t disk_sync(struct disk *dev) {
    return 0;
}

diskrws_t disk_wrs = {
    .read = disk_read,
    .write = disk_write,
    .sync = disk_sync,
};

struct disk* disk_mount(const char* path, size_t bsize, size_t bcount) {
    if (!path || !bsize || !bcount) 
        return NULL;

    struct disk *d = malloc(sizeof(struct disk));
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

void disk_umount(struct disk* d) {
    if (!d)
        return;
    
    close(d->fd);
    free(d);
}

void disk_init(disk_mgr_t* mangr) {
    memset(mangr, 0, sizeof(disk_mgr_t));
}

size_t disk_register(disk_mgr_t* mangr, struct disk* disk, diskrws_t* rws) {
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

struct disk* disk_get(disk_mgr_t* disk_mgr, int id) {
    if (id < 0 || id >= MAX_DISKS) {
        return NULL;
    }

    struct disk* disk = disk_mgr->disks[id];

    return disk;
}