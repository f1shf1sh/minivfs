#pragma once

#include <sys/types.h>
#include <pthread.h>
#include "defs.h"
#include "vdev/vdev.h"
#include "fs/inode.h"
#include "spinlock.h"

/*
        [ boot block | sb | log(ignore) | innode bitmap | inode blocks | free bit map | data blocks ]
        ┌────────────┐ 0
        │ boot block │
        ├────────────┤ 1
        │ superblock │
        ├────────────┤ 2–33
        │    log     │
        ├────────────┤ 34–35
        │ inode map  │
        ├────────────┤ 36–163
        │ inode tbl  │
        ├────────────┤ 164–165
        │ data map   │
        ├────────────┤ 166–end
        │ data blks  │
        └────────────┘
*/

#define FS_MAGIC 0x68736966
#define BOOT_BLOCKS 1
#define SUPER_BLOCKS 1
#define LOG_BLOCKS 32
#define INODE_BITMAP_BLOCKS 2
#define INODE_TABLE_BLOCKS 128
#define DATA_BITMAP_BLOCKS 2
#define ROOT_INODE 1

typedef struct superblock {
    uint32_t magic;            // 文件系统标识
    uint32_t total_blocks;     // 总块数
    uint32_t block_size;       // 每块大小（如 4096）
    
    uint32_t log_start;        // log 起始块号
    uint32_t log_blocks;       // log 占用块数

    uint32_t inode_map_start;  // inode bitmap 起始块
    uint32_t inode_map_blocks; // inode bitmap 块数

    uint32_t inode_start;      // inode table 起始块
    uint32_t inode_blocks;     // inode table 块数

    uint32_t data_map_start;   // data bitmap 起始块
    uint32_t data_map_blocks;  // data bitmap 块数

    uint32_t data_start;       // data 区起始块
    uint32_t data_blocks;      // data 区块数

    uint32_t root_inode;       // 根目录 inode 号（通常 0 或 1）
    uint32_t reserved[8];      // 保留以便未来扩展
} sb_t;

/*
==================================================================
                fs struct and interface
==================================================================
*/
// typedef struct icache icache_t;


typedef struct fs {
    vdev_t *vdev;
    struct superblock sb;
    icache_mgr_t cache_mgr;
    uint32_t icache_size; // inode cache size default is 64
    uint8_t *inode_bitmap;
    uint8_t *data_bitmap;

    uint32_t root_inum;
    uint32_t cwd_inum;
    

    lock_t lock_bmap;
    lock_t lock_imap;
    pthread_mutex_t fs_lock;
}fs_t;

// ---------- fs interface ----------
int fs_mount(fs_t *fs, vdev_t *vdev, uint32_t icache_size);
int fs_sync(fs_t *fs);
int fs_unmount(fs_t *fs);
