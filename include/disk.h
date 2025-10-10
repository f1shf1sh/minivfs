#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>
#include "defs.h"


typedef struct disk {
    int fd; // disk file fd
    char name[64]; // disk path
    size_t bsize;
    size_t bcount;
    diskrws_t* rws;
    void* privt;
    pthread_mutex_t lock;
} disk_t;

typedef struct {
    ssize_t (*read)(disk_t*, void*, size_t, off_t);
    ssize_t (*write)(disk_t*, const void*, size_t, off_t);
    size_t (*sync)(disk_t*);
} diskrws_t;

typedef struct {
    disk_t *disks[MAX_DISKS];
    diskrws_t *ops[MAX_DISKS];
    size_t count;
    pthread_mutex_t lock;
} disk_mangr_t;


void disk_init(disk_mangr_t*);
size_t dev_register(disk_mangr_t*, disk_t*, diskrws_t*);
disk_t* disk_get(disk_mangr_t*, int);

disk_t* disk_mount(const char*, size_t, size_t);
void disk_umount(disk_t*);

ssize_t disk_read(disk_t*, void*, size_t, off_t);
ssize_t disk_write(disk_t*, const void*, size_t, off_t);
size_t disk_sync(disk_t*);

#endif