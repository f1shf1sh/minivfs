#include "fs/fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int valid_layout(const sb_t *sb) {
    return sb->magic == FS_MAGIC && sb->block_size == BSIZE &&
           sb->total_blocks >= MIN_IMAGE_BLOCKS &&
           sb->total_blocks <= MAX_IMAGE_BLOCKS &&
           sb->log_start == LOG_START && sb->log_blocks == LOG_BLOCKS &&
           sb->inode_map_start == INODE_MAP_START &&
           sb->inode_map_blocks == INODE_BITMAP_BLOCKS &&
           sb->inode_start == INODE_TABLE_START &&
           sb->inode_blocks == INODE_TABLE_BLOCKS &&
           sb->data_map_start == DATA_MAP_START &&
           sb->data_map_blocks == DATA_BITMAP_BLOCKS &&
           sb->data_start == DATA_START &&
           sb->data_blocks == sb->total_blocks - sb->data_start &&
           sb->inode_blocks * INODES_PER_BLOCK <= sb->inode_map_blocks * BSIZE * 8 &&
           sb->data_blocks <= sb->data_map_blocks * BSIZE * 8 &&
           sb->root_inode == ROOT_INODE;
}

static void release_resources(fs_t *fs) {
    for (uint32_t i = 0; i < ICACHE_SIZE; i++)
        free(fs->cache_mgr.slots[i].indirect);
    free(fs->inode_bitmap);
    free(fs->data_bitmap);
    memset(fs, 0, sizeof(*fs));
}

int fs_mount(fs_t *fs, vdev_t *vdev, uint32_t icache_size) {
    if (icache_size != 0 && icache_size != ICACHE_SIZE) {
        errno = EINVAL;
        return -1;
    }
    memset(fs, 0, sizeof(*fs));
    fs->vdev = vdev;
    fs->icache_size = ICACHE_SIZE;

    uint8_t buf[BSIZE];
    if (vdev->ops.read(vdev->priv, buf, BOOT_BLOCKS) < 0)
        goto fail;
    memcpy(&fs->sb, buf, sizeof(fs->sb));
    if (!valid_layout(&fs->sb)) {
        errno = EINVAL;
        goto fail;
    }
    /* Validate declared capacity through the device interface. */
    if (vdev->ops.read(vdev->priv, buf, fs->sb.total_blocks - 1) < 0)
        goto fail;

    fs->inode_bitmap = malloc((size_t)fs->sb.inode_map_blocks * BSIZE);
    fs->data_bitmap = malloc((size_t)fs->sb.data_map_blocks * BSIZE);
    if (fs->inode_bitmap == NULL || fs->data_bitmap == NULL)
        goto fail;
    for (uint32_t i = 0; i < fs->sb.inode_map_blocks; i++) {
        if (vdev->ops.read(vdev->priv, fs->inode_bitmap + i * BSIZE,
                          fs->sb.inode_map_start + i) < 0)
            goto fail;
    }
    for (uint32_t i = 0; i < fs->sb.data_map_blocks; i++) {
        if (vdev->ops.read(vdev->priv, fs->data_bitmap + i * BSIZE,
                          fs->sb.data_map_start + i) < 0)
            goto fail;
    }
    if (!BIT_TST(fs->inode_bitmap, 0) ||
        !BIT_TST(fs->inode_bitmap, fs->sb.root_inode) ||
        !BIT_TST(fs->data_bitmap, 0)) {
        errno = EINVAL;
        goto fail;
    }

    fs->root_inum = fs->sb.root_inode;
    fs->cwd_inum = fs->root_inum;
    icache_t *root;
    if (iget(fs, fs->root_inum, &root) < 0)
        goto fail;
    if (root->inode.type != TYPE_DIR || root->inode.links == 0) {
        errno = EINVAL;
        goto fail;
    }
    if (iput(fs, root) < 0)
        goto fail;
    return 0;

fail:
    release_resources(fs);
    return -1;
}

int fs_sync(fs_t *fs) {
    if (fs->vdev == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (icache_sync(fs) < 0)
        return -1;
    for (uint32_t i = 0; i < fs->sb.inode_map_blocks; i++) {
        if (fs->vdev->ops.write(fs->vdev->priv, fs->inode_bitmap + i * BSIZE,
                               fs->sb.inode_map_start + i) < 0)
            return -1;
    }
    for (uint32_t i = 0; i < fs->sb.data_map_blocks; i++) {
        if (fs->vdev->ops.write(fs->vdev->priv, fs->data_bitmap + i * BSIZE,
                               fs->sb.data_map_start + i) < 0)
            return -1;
    }
    return fs->vdev->ops.sync ? fs->vdev->ops.sync(fs->vdev->priv) : 0;
}

int fs_unmount(fs_t *fs) {
    if (fs_sync(fs) < 0)
        return -1;
    release_resources(fs);
    return 0;
}
