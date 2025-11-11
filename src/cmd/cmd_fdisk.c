// cmd_fdisk.c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fs/fs.h"
#include "user.h"

extern ctx_t *get_ctx(void);

int cmd_fdisk(int argc, char **argv) {
    ctx_t *ctx = get_ctx();
    if (!ctx) {
        fprintf(stderr, "Filesystem context not initialized\n");
        return -1;
    }

    fs_t *fs = &ctx->fs;
    sb_t *sb = &fs->sb;

    printf("Disk layout for %s:\n", "disk.img");
    printf("----------------------------------------\n");
    printf("Boot Block        : %u\n", 0);
    printf("Super Block       : %u\n", sb->inode_start - SUPER_BLOCKS - LOG_BLOCKS - INODE_BITMAP_BLOCKS);
    printf("Log Blocks        : %u - %u\n", sb->log_start, sb->log_start + sb->log_blocks - 1);
    printf("Inode Bitmap      : %u - %u\n", sb->inode_map_start, sb->inode_map_start + sb->inode_map_blocks - 1);
    printf("Inode Table       : %u - %u\n", sb->inode_start, sb->inode_start + sb->inode_blocks - 1);
    printf("Data Bitmap       : %u - %u\n", sb->data_map_start, sb->data_map_start + sb->data_map_blocks - 1);
    printf("Data Blocks       : %u - %u\n", sb->data_start, sb->data_start + sb->data_blocks - 1);
    printf("Block size        : %u bytes\n", BSIZE);
    printf("Total inodes      : %u\n", sb->inode_blocks * (BSIZE / sizeof(inode_t)));

    // Count used inodes
    uint32_t used_inodes = 0;
    for (uint32_t i = 0; i < sb->inode_blocks*(BSIZE / sizeof(inode_t)); i++) {
        if (BIT_TST(fs->inode_bitmap, i)) 
            used_inodes++;
    }

    // Count used data blocks
    uint32_t used_data = 0;
    for (uint32_t i = 0; i < sb->data_blocks; i++) {
        if (BIT_TST(fs->data_bitmap, i)) 
            used_data++;
    }

    printf("Used inodes       : %u\n", used_inodes);
    printf("Used data blocks  : %u\n", used_data);
    printf("Free data blocks  : %u\n", sb->data_blocks - used_data);

    return 0;
}