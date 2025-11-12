#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "fs/fs.h"
#include "spinlock.h"

/*
    return 0 is success, -1 is fail
*/
int fs_mount(fs_t *fs, vdev_t *vdev, uint32_t icache_size) {
    if (!fs || !vdev)
        return -1;
    fs->vdev = vdev;
    fs->icache_size = (icache_size == 0) ? ICACHE_SIZE : icache_size;

    // read superblock into memory
    uint8_t *buf = malloc(BSIZE);
    if (!buf)
        return -1;
    if(vdev->ops.read(vdev->priv, buf, 1) != 0) {
        free(buf);
        return -1;
    }
    memcpy(&fs->sb, buf, sizeof(sb_t));

    if (fs->sb.magic != FS_MAGIC)
        return -1;
    if (fs->sb.block_size != BSIZE) {
        if (fs->sb.block_size != 0) 
            return -1;
    }

    // load bitmaps into memory
    int inode_bitmap_bytes = fs->sb.inode_map_blocks * BSIZE;
    int data_bitmap_bytes = fs->sb.data_map_blocks * BSIZE;
    fs->inode_bitmap = malloc(inode_bitmap_bytes);
    fs->data_bitmap = malloc(data_bitmap_bytes);
    if (!fs->inode_bitmap || !fs->data_bitmap) {
        free(fs->inode_bitmap);
        free(fs->data_bitmap);
        return -1;
    }

    // read inode bitmap
    for (uint32_t i = 0; i < fs->sb.inode_map_blocks; i++) {
        if (vdev->ops.read(vdev->priv, fs->inode_bitmap + i*BSIZE, fs->sb.inode_map_start + i) != 0) {
            free(fs->inode_bitmap);
            free(fs->data_bitmap);
            return -1;
        }
    }

    // read data bitmap
    for (uint32_t i = 0; i < fs->sb.data_map_blocks; i++) {
        if (vdev->ops.read(vdev->priv, fs->data_bitmap + i*BSIZE, fs->sb.data_map_start+i) != 0) {
            free(fs->inode_bitmap);
            free(fs->data_bitmap);
            return -1;
        }
    }

    // init mutex lock
    lock_init(&fs->lock_bmap);
    lock_init(&fs->lock_imap);
    pthread_mutex_init(&fs->fs_lock, NULL);


    // init inode cache
    for (uint32_t i = 0; i < fs->icache_size; i++) {
        lock_init(&fs->cache_mgr.slots[i].lock);
    }
    pthread_mutex_init(&fs->cache_mgr.lock, NULL);
    
    // init cwd
    fs->root_inum = 1;
    fs->cwd_inum = ROOT_INODE;
    printf("[file system] init fs cxt\n");
    return 0;
}

int fs_unmount(fs_t *fs) {
    if (!fs) {
        return -1;
    }

    if (fs_sync(fs) < 0) {
        return -1;
    }

    if (fs->inode_bitmap) {
        free(fs->inode_bitmap);
    }

    if (fs->data_bitmap) {
        free(fs->data_bitmap);
    }


    pthread_mutex_destroy(&fs->cache_mgr.lock);
    pthread_mutex_destroy(&fs->fs_lock);
   
    free(fs->vdev->priv);
    return 0;
}

int fs_sync(fs_t *fs) {
    if(!fs)
        return -1;

    // write inode bitmap
    for (uint32_t i = 0; i < fs->sb.inode_map_blocks; i++) {
        if (fs->vdev->ops.write(fs->vdev->priv, fs->inode_bitmap + i*BSIZE, fs->sb.inode_map_start+i) != 0) {
            return -1;
        }
    }

    // write data bitmap
    for (uint32_t i = 0; i < fs->sb.data_map_blocks; i++) {
        if (fs->vdev->ops.write(fs->vdev->priv, fs->data_bitmap+i*BSIZE, fs->sb.data_map_start+i) != 0) {
            return -1;
        }
    }

    // write inode cache into memory
    for (uint32_t i = 0; i < fs->icache_size; i++) {
        icache_t *icache = &fs->cache_mgr.slots[i];
        if (icache->inum != 0 && icache->dirty == 1) {
            if (iwrite(fs, icache->inum, icache)) {
                return -1;
            }
            icache->dirty = 0;
        }
    }

    // write superblock
    // TODO

    // sync vdev
    if (fs->vdev->ops.sync) {
        fs->vdev->ops.sync(fs->vdev->priv);
    }

    return 0;
}
