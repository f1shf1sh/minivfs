#include "fs/fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int iread(fs_t *fs, uint32_t inum, icache_t *ic) {
    if (inum == 0 || inum >= fs->sb.inode_blocks * INODES_PER_BLOCK) {
        errno = EINVAL;
        return -1;
    }

    uint8_t buf[BSIZE];
    if (fs->vdev->ops.read(fs->vdev->priv, buf,
                          fs->sb.inode_start + INODE_BLOCK(inum)) < 0)
        return -1;
    inode_t inode;
    memcpy(&inode, buf + INODE_OFFSET(inum) * sizeof(inode_t), sizeof(inode));
    if (inode.type > TYPE_DIR || inode.size > MAX_FILE_SIZE) {
        errno = EIO;
        return -1;
    }
    for (uint32_t i = 0; i <= NDIRECT; i++) {
        if (inode.direct[i] >= fs->sb.data_blocks) {
            errno = EIO;
            return -1;
        }
    }

    uint32_t *indirect = NULL;
    if (inode.direct[NDIRECT] != 0) {
        indirect = malloc(BSIZE);
        if (indirect == NULL)
            return -1;
        if (fs->vdev->ops.read(fs->vdev->priv, indirect,
                              fs->sb.data_start + inode.direct[NDIRECT]) < 0) {
            free(indirect);
            return -1;
        }
        for (uint32_t i = 0; i < INDIRECT_ENTRIES; i++) {
            if (indirect[i] >= fs->sb.data_blocks) {
                free(indirect);
                errno = EIO;
                return -1;
            }
        }
    }
    ic->inode = inode;
    ic->indirect = indirect;
    ic->dirty = 0;
    return 0;
}

int iwrite(fs_t *fs, uint32_t inum, icache_t *ic) {
    uint8_t buf[BSIZE];
    uint32_t block = fs->sb.inode_start + INODE_BLOCK(inum);
    if (fs->vdev->ops.read(fs->vdev->priv, buf, block) < 0)
        return -1;
    if (ic->inode.direct[NDIRECT] != 0 &&
        fs->vdev->ops.write(fs->vdev->priv, ic->indirect,
                           fs->sb.data_start + ic->inode.direct[NDIRECT]) < 0)
        return -1;
    memcpy(buf + INODE_OFFSET(inum) * sizeof(inode_t), &ic->inode, sizeof(ic->inode));
    if (fs->vdev->ops.write(fs->vdev->priv, buf, block) < 0)
        return -1;
    ic->dirty = 0;
    return 0;
}

int itruncate(fs_t *fs, icache_t *ic) {
    inode_t old_inode = ic->inode;
    uint32_t *old_indirect = ic->indirect;
    ic->inode.size = 0;
    memset(ic->inode.direct, 0, sizeof(ic->inode.direct));
    ic->indirect = NULL;
    ic->dirty = 1;

    /* Detach references on disk before making their blocks reusable. */
    if (iwrite(fs, ic->inum, ic) < 0) {
        ic->inode = old_inode;
        ic->indirect = old_indirect;
        ic->dirty = 1;
        return -1;
    }
    int result = 0;
    for (uint32_t i = 0; i < NDIRECT; i++) {
        if (old_inode.direct[i] != 0 && bfree(fs, old_inode.direct[i]) < 0)
            result = -1;
    }
    if (old_indirect != NULL) {
        for (uint32_t i = 0; i < INDIRECT_ENTRIES; i++) {
            if (old_indirect[i] != 0 && bfree(fs, old_indirect[i]) < 0)
                result = -1;
        }
        if (bfree(fs, old_inode.direct[NDIRECT]) < 0)
            result = -1;
        free(old_indirect);
    }
    return result;
}
