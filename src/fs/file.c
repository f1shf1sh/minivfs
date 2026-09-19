#include "fs/fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int bget(fs_t *fs, icache_t *ic, uint32_t index, int allocate) {
    if (index >= NDIRECT + INDIRECT_ENTRIES) {
        errno = EFBIG;
        return -1;
    }
    if (index < NDIRECT) {
        if (ic->inode.direct[index] == 0 && allocate) {
            int block = balloc(fs);
            if (block < 0)
                return -1;
            ic->inode.direct[index] = (uint32_t)block;
            ic->dirty = 1;
        }
        return (int)ic->inode.direct[index];
    }

    index -= NDIRECT;
    if (ic->inode.direct[NDIRECT] == 0) {
        if (!allocate)
            return 0;
        uint32_t *indirect = calloc(1, BSIZE);
        if (indirect == NULL)
            return -1;
        int block = balloc(fs);
        if (block < 0) {
            free(indirect);
            return -1;
        }
        ic->inode.direct[NDIRECT] = (uint32_t)block;
        ic->indirect = indirect;
        ic->dirty = 1;
    }
    if (ic->indirect[index] == 0 && allocate) {
        int block = balloc(fs);
        if (block < 0)
            return -1;
        ic->indirect[index] = (uint32_t)block;
        ic->dirty = 1;
    }
    return (int)ic->indirect[index];
}

int bread(fs_t *fs, icache_t *ic, void *buffer, uint32_t size, uint32_t offset) {
    if (offset >= ic->inode.size || size == 0)
        return 0;
    uint32_t wanted = MIN(size, ic->inode.size - offset);
    uint32_t done = 0;
    uint8_t blockbuf[BSIZE];
    while (done < wanted) {
        uint32_t position = offset + done;
        uint32_t within = position % BSIZE;
        uint32_t count = MIN(BSIZE - within, wanted - done);
        int block = bget(fs, ic, position / BSIZE, 0);
        if (block <= 0) {
            if (block == 0)
                errno = EIO;
            break;
        }
        if (fs->vdev->ops.read(fs->vdev->priv, blockbuf,
                              fs->sb.data_start + (uint32_t)block) < 0)
            break;
        memcpy((char *)buffer + done, blockbuf + within, count);
        done += count;
    }
    return done != 0 ? (int)done : -1;
}

int bwrite(fs_t *fs, icache_t *ic, const void *buffer,
           uint32_t size, uint32_t offset, int update_size) {
    if (offset > ic->inode.size) {
        errno = EINVAL;
        return -1;
    }
    if (size == 0)
        return 0;
    if (offset >= MAX_FILE_SIZE) {
        errno = EFBIG;
        return -1;
    }

    uint32_t wanted = MIN(size, (uint32_t)MAX_FILE_SIZE - offset);
    uint32_t done = 0;
    uint8_t blockbuf[BSIZE];
    while (done < wanted) {
        uint32_t position = offset + done;
        uint32_t within = position % BSIZE;
        uint32_t count = MIN(BSIZE - within, wanted - done);
        int block = bget(fs, ic, position / BSIZE, 1);
        if (block < 0)
            break;
        if ((within != 0 || count != BSIZE) &&
            fs->vdev->ops.read(fs->vdev->priv, blockbuf,
                              fs->sb.data_start + (uint32_t)block) < 0)
            break;
        memcpy(blockbuf + within, (const char *)buffer + done, count);
        if (fs->vdev->ops.write(fs->vdev->priv, blockbuf,
                               fs->sb.data_start + (uint32_t)block) < 0)
            break;
        done += count;
    }
    if (done != 0) {
        if (update_size && offset + done > ic->inode.size)
            ic->inode.size = offset + done;
        ic->dirty = 1;
        return (int)done;
    }
    return -1;
}
