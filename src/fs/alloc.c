#include "fs/fs.h"

#include <errno.h>

int balloc(fs_t *fs) {
    for (uint32_t block = 1; block < fs->sb.data_blocks; block++) {
        if (BIT_TST(fs->data_bitmap, block))
            continue;
        uint8_t zero[BSIZE] = {0};
        if (fs->vdev->ops.write(fs->vdev->priv, zero, fs->sb.data_start + block) < 0)
            return -1;
        BIT_SET(fs->data_bitmap, block);
        return (int)block;
    }
    errno = ENOSPC;
    return -1;
}

int bfree(fs_t *fs, uint32_t block) {
    if (block == 0 || block >= fs->sb.data_blocks) {
        errno = EINVAL;
        return -1;
    }
    BIT_CLR(fs->data_bitmap, block);
    return 0;
}

int ialloc(fs_t *fs) {
    uint32_t count = fs->sb.inode_blocks * INODES_PER_BLOCK;
    for (uint32_t inum = 1; inum < count; inum++) {
        if (BIT_TST(fs->inode_bitmap, inum))
            continue;
        icache_t empty = {0};
        if (iwrite(fs, inum, &empty) < 0)
            return -1;
        BIT_SET(fs->inode_bitmap, inum);
        return (int)inum;
    }
    errno = ENOSPC;
    return -1;
}

int ifree(fs_t *fs, uint32_t inum) {
    if (inum <= ROOT_INODE || inum >= fs->sb.inode_blocks * INODES_PER_BLOCK) {
        errno = EINVAL;
        return -1;
    }
    BIT_CLR(fs->inode_bitmap, inum);
    return 0;
}
