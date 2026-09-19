#pragma once

#include "fs/format.h"
#include "fs/inode.h"
#include "vdev/vdev.h"

/* Public my_* operations serialize access to this mounted filesystem. */
struct fs {
    vdev_t *vdev;             /* Borrowed; its creator owns the device. */
    sb_t sb;
    icache_mgr_t cache_mgr;
    uint32_t icache_size;
    uint8_t *inode_bitmap;
    uint8_t *data_bitmap;
    uint32_t root_inum;
    uint32_t cwd_inum;
};

int fs_mount(fs_t *fs, vdev_t *vdev, uint32_t icache_size);
int fs_sync(fs_t *fs);
int fs_unmount(fs_t *fs);
