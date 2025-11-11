#pragma once

// #include "fs.h"
#include <pthread.h>
#include <stdint.h>

#define INODE_SIZE 128
#define NDIRECT 12
#define INODE_NINDIRECT 1
#define ICACHE_SIZE 64 // inode cache size
#define ICACHE_HASH(inum) ((inum) % (ICACHE_SIZE))
#define INODES_PER_BLOCK (BSIZE / sizeof(inode_t))
#define INODE_BLOCK(inum) ((inum) / INODES_PER_BLOCK)
#define INODE_OFFSET(inum) ((inum) % INODES_PER_BLOCK)

typedef enum {
    TYPE_FREE = 0,
    TYPE_FILE = 1,
    TYPE_DIR  = 2
} inode_type_t;

// inode struct
// sizoef(inode_t) = 128
typedef struct inode {
    uint16_t type;   // 0=free, 1=file, 2=dir
    uint16_t links;
    uint32_t size;
    uint32_t direct[NDIRECT+1];
    uint8_t  pad[4];
}inode_t;

typedef struct icache {
    uint32_t inum;
    inode_t  inode;
    uint32_t *indirect;
    int refcnt;
    int dirty;
    pthread_rwlock_t lock;
} icache_t;

typedef struct icache_mgr {
    icache_t slots[ICACHE_SIZE];
    pthread_mutex_t lock;    // 全局hash桶锁（保护哈希表操作）
} icache_mgr_t;

/* 
=========================== INODE LAYER ==========================
    define inode struct and interface
==================================================================
*/
struct fs;
typedef struct fs fs_t;

// ops data bitmap
int balloc(fs_t *fs);
int bfree(fs_t *fs, uint32_t blkno);

int bget(fs_t *fs, icache_t *ic, uint32_t idx, int alloc);
int bread(fs_t *fs, icache_t *ic, void *buf, uint32_t size, uint32_t offset);
int bwrite(fs_t *fs, icache_t *ic, const void *buf, uint32_t size, uint32_t offset, int update);

// ops inode bitmap
int ialloc(fs_t *fs);
int ifree(fs_t *fs, uint32_t inum);

// inode cache interface
int iget(fs_t *fs, uint32_t inum, icache_t **ic);
int iput(fs_t *fs, icache_t *ic);

// alloc inode
int iread(fs_t *fs, uint32_t inum, icache_t *ic);
int iwrite(fs_t *fs, uint32_t inum, const icache_t *ic);

