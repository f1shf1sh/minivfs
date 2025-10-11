#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>
#include "defs.h"

struct disk; 

typedef struct {
    ssize_t (*read)(struct disk*, void*, size_t, off_t);
    ssize_t (*write)(struct disk*, const void*, size_t, off_t);
    size_t (*sync)(struct disk*);
} diskrws_t;

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
    struct disk *disks[MAX_DISKS];
    diskrws_t *ops[MAX_DISKS];
    size_t count;
    pthread_mutex_t lock;
} disk_mgr_t;

// global var to manager disk
extern disk_mgr_t disk_mgr;

void disk_init(disk_mgr_t*);
size_t disk_register(disk_mgr_t*, struct disk*, diskrws_t*);
struct disk* disk_get(disk_mgr_t*, int);

struct disk* disk_mount(const char*, size_t, size_t);
void disk_umount(struct disk*);

ssize_t disk_read(struct disk*, void*, size_t, off_t);
ssize_t disk_write(struct disk*, const void*, size_t, off_t);
size_t disk_sync(struct disk*);

#endif