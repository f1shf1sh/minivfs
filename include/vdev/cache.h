#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "defs.h"
#include "vdev.h"
#include "disk.h"
#include "spinlock.h"

/*
cache buffer layer
cache use lru, the max cache block be defined in NBUF
*/

#define DIRTY 1
#define VALID 2
#define NBUF 128 // size of cache block  

typedef struct bcache {
    int block_no;  // block num in img, offset = block_no * block_size
    unsigned char data[BSIZE]; // block data
    unsigned char flags;     // have two type dirty and valid
    int refcnt;       
    struct bcache *prev; 
    struct bcache *next;
    lock_t lock;
} bcache_t;

typedef struct bcache_mgr {
    bcache_t blocks[NBUF];
    vdev_t *backend;
    vdev_t vdev;
    bcache_t *lru_head;
    bcache_t *lru_tail;
} bcache_mgr_t;


int cread(void *priv, void *buf, unsigned int count);
int cwrite(void *priv, const void *buf, unsigned int count);
int csync(void *priv);
int cget(bcache_mgr_t *mgr, int block_no, bcache_t **bc);
int cput(bcache_mgr_t *mgr, bcache_t *bc);

// LRU support 
bcache_t* lookup_block(bcache_mgr_t *mgr, int dev_id, int block_no);
void lru_append(bcache_mgr_t *mgr, bcache_t *blk);
void lru_remove(bcache_mgr_t *mgr, bcache_t *blk);
bcache_t *evict_block(bcache_mgr_t *mgr);


// cache layer interface
int cache_init(bcache_mgr_t *mgr, vdev_t *dev);
int cache_destroy(bcache_mgr_t *mgr);




