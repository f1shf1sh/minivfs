#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "fs/inode.h"
#include "fs/fs.h"

int balloc(fs_t *fs) {
    if (!fs)
        return -1;
    pthread_mutex_lock(&fs->data_bitmap_lock);
    for (uint32_t i = 0; i < fs->sb.data_blocks; i++) {
        if (!BIT_TST(fs->data_bitmap, i)) {
            BIT_SET(fs->data_bitmap, i);
            pthread_mutex_unlock(&fs->data_bitmap_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&fs->data_bitmap_lock);
    return -1;
}

int bfree(fs_t *fs, uint32_t blkno) {
    if (!fs)
        return -1;

    if (blkno < fs->sb.data_start || blkno > fs->sb.data_start + fs->sb.data_blocks) 
        return -1;
    
    pthread_mutex_lock(&fs->data_bitmap_lock);
    BIT_CLR(fs->data_bitmap, blkno);
    pthread_mutex_unlock(&fs->data_bitmap_lock);

    return 0;
}

int ialloc(fs_t *fs) {
    if (!fs)
        return -1;
    pthread_mutex_lock(&fs->inode_bitmap_lock);
    for (int i = 0; i < fs->sb.inode_blocks * INODES_PER_BLOCK; i++) {
        if (!BIT_TST(fs->inode_bitmap, i)) {
            BIT_SET(fs->inode_bitmap, i);
            pthread_mutex_unlock(&fs->inode_bitmap_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&fs->inode_bitmap_lock);
    return -1;
}

int ifree(fs_t *fs, uint32_t inum) {
    if (!fs)
        return -1;
    pthread_mutex_lock(&fs->inode_bitmap_lock);
    BIT_CLR(fs->inode_bitmap, inum);
    pthread_mutex_unlock(&fs->inode_bitmap_lock);
    return 0;
}

// 读取inode节点信息
int iread(fs_t *fs, uint32_t inum, inode_t *ino) {
    if (!fs || !ino) 
        return -1;
    
    int off = INODE_BLOCK(inum);
    int idx = INODE_OFFSET(inum);
    uint8_t *buf = malloc(BSIZE);
    if (!buf)
        return -1;
    int r = fs->vdev->ops.read(fs->vdev->priv, buf, fs->sb.inode_start+off);
    if (r) {
        free(buf);
        return -1;
    }
    memcpy(ino, buf + idx*sizeof(inode_t), sizeof(inode_t));

    free(buf);
    return 0;
}

// 写回inode节点信息
int iwrite(fs_t *fs, uint32_t inum, const inode_t *ino) {
    if (!fs || !ino) 
        return -1;

    int off = INODE_BLOCK(inum);
    int idx = INODE_OFFSET(inum);
    uint8_t *buf = malloc(BSIZE);
    if (!buf)
        return -1;
    int r = fs->vdev->ops.read(fs->vdev->priv, buf, fs->sb.inode_start+off);
    if (r) {
        free(buf);
        return -1;
    }
    
    memcpy(buf+idx*sizeof(inode_t), ino, sizeof(inode_t));
    fs->vdev->ops.write(fs->vdev->priv, buf, fs->sb.inode_start+off);
    free(buf);

    return 0;
}

int bget(fs_t *fs, icache_t *ic, uint32_t idx, int alloc) {
    int blkno = 0;

    // if (alloc)
    //     pthread_rwlock_wrlock(&ic->lock);
    // else
    //     pthread_rwlock_rdlock(&ic->lock);

    if (idx < INODE_NDIRECT) {
        blkno = ic->inode.direct[idx];
        if (!blkno && alloc) {
            blkno = balloc(fs);
            if (blkno != -1) {
                ic->inode.direct[idx] = blkno;
                ic->dirty = 1;
            }
        }
        // pthread_rwlock_unlock(&ic->lock);
        return blkno;
    }

    idx -= INODE_NDIRECT;
    if (!ic->inode.direct[INODE_NDIRECT+1] && alloc) {
        int indirect_blk = balloc(fs);
        if (indirect_blk == -1) {
            // pthread_rwlock_unlock(&ic->lock);
            return -1;
        }
        uint8_t zero_buf[BSIZE] = {0};
        fs->vdev->ops.write(fs->vdev->priv, zero_buf, indirect_blk);
        ic->inode.direct[INODE_NDIRECT+1] = indirect_blk;
        ic->dirty = 1;
    }

    // pthread_rwlock_unlock(&ic->lock);

    if (!ic->inode.direct[INODE_NDIRECT+1])
        return 0;

    uint32_t *table = malloc(BSIZE);
    if (!table)
        return -1;
    fs->vdev->ops.read(fs->vdev->priv, table, ic->inode.direct[INODE_NDIRECT+1]);
    blkno = table[idx];

    if (!blkno && alloc) {
        blkno = balloc(fs);
        if (blkno != -1) {
            table[idx] = blkno;
            fs->vdev->ops.write(fs->vdev->priv, table, ic->inode.direct[INODE_NDIRECT+1]);
        }
    }

    free(table);
    return blkno;
}

/*
    buf: user data buf
    size: read size
    offset: start size in file
*/
int bread(fs_t *fs, icache_t *ic, void *buf, uint32_t size, uint32_t offset) {
     if (!fs || !ic || !buf)
        return -1;

    // pthread_rwlock_rdlock(&ic->lock);

    if (offset >= ic->inode.size) {
        // pthread_rwlock_unlock(&ic->lock);
        return 0; // EOF
    }

    if (offset + size > ic->inode.size)
        size = ic->inode.size - offset;

    uint32_t remain = size;
    uint32_t block_idx = offset / BSIZE;
    uint32_t block_off = offset % BSIZE;
    uint8_t blkbuf[BSIZE];

    uint8_t *p = (uint8_t *)buf;

    while (remain > 0) {
        int blkno = bget(fs, ic, block_idx, 0);
        if (blkno <= 0)
            break;

        if (fs->vdev->ops.read(fs->vdev->priv, blkbuf, fs->sb.data_start+blkno))
            break;

        uint32_t to_copy = MIN(BSIZE - block_off, remain);
        memcpy(p, blkbuf + block_off, to_copy);

        remain -= to_copy;
        p += to_copy;
        block_idx++;
        block_off = 0;
    }

    // pthread_rwlock_unlock(&ic->lock);
    return size - remain;
}

/*
    buf: user data buf
    size: user data buf size
    offset: disk data blk offset
*/
int bwrite(fs_t *fs, icache_t *ic, const void *buf, uint32_t size, uint32_t offset) {
    if (!fs || !ic || !buf)
        return -1;

    // pthread_rwlock_wrlock(&ic->lock);

    uint32_t remain = size;
    uint32_t block_idx = offset / BSIZE;
    uint32_t block_off = offset % BSIZE;
    uint8_t blkbuf[BSIZE];

    const uint8_t *p = (const uint8_t *)buf;

    while (remain > 0) {
        int blkno = bget(fs, ic, block_idx, 1); // alloc if missing
        if (blkno <= 0)
            break;

        if (fs->vdev->ops.read(fs->vdev->priv, blkbuf, fs->sb.data_start+blkno))
            break;

        uint32_t to_copy = MIN(BSIZE - (int)block_off, (int)remain);
        memcpy(blkbuf + block_off, p, to_copy);
        fs->vdev->ops.write(fs->vdev->priv, blkbuf, fs->sb.data_start+blkno);

        remain -= to_copy;
        p += to_copy;
        block_idx++;
        block_off = 0;
    }

    uint32_t new_size = offset + size;
    if (new_size > ic->inode.size)
        ic->inode.size = new_size;

    // pthread_rwlock_unlock(&ic->lock);
    return size - remain;
}

int iget(fs_t *fs, uint32_t inum, icache_t **ic) {
    uint32_t h = ICACHE_HASH(inum);

    // pthread_mutex_lock(&fs->cache_mgr.lock);
    // 线性探测
    for (int i = 0; i < ICACHE_SIZE; i++) {
        uint32_t idx = (h + i) % ICACHE_SIZE;
        icache_t *slot = &fs->cache_mgr.slots[idx];

        if (slot->inum == inum) { // 命中缓存
            slot->refcnt++;
            *ic = slot;
            // pthread_mutex_unlock(&fs->cache_mgr.lock);
            return 0;
        }

        if (slot->inum == 0) { // 空位，分配新缓存
            if (iread(fs, inum, &slot->inode) != 0) {
                // pthread_mutex_unlock(&fs->cache_mgr.lock);
                return -1;
            }

            slot->inum = inum;
            slot->refcnt = 1;
            slot->dirty = 0;
            *ic = slot;
            // pthread_mutex_unlock(&fs->cache_mgr.lock);
            return 0;
        }
    }

    // pthread_mutex_unlock(&fs->cache_mgr.lock);
    return -1;
}

int iput(fs_t *fs, icache_t **ic) {
    // pthread_mutex_lock(&fs->cache_mgr.lock);
    (*ic)->refcnt--;
    if ((*ic)->refcnt == 0 && (*ic)->dirty) {
        iwrite(fs, (*ic)->inum, &(*ic)->inode);
        (*ic)->dirty = 0;
    }
    // pthread_mutex_unlock(&fs->cache_mgr.lock);
    return 0;
}