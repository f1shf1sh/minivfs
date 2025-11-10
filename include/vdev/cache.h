#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>

#include "defs.h"
#include "vdev.h"
#include "disk.h"
/*
cache buffer layer
cache use lru, the max cache block be defined in NBUF
*/

#define DIRTY 1
#define VALID 2
#define NBUF 128 // size of cache block  

typedef struct cache_block {
    int dev_id;    // device id 
    int block_no;  // block num in img, offset = block_no * block_size
    unsigned char data[BSIZE]; // block data
    unsigned char flags;     // have two type dirty and valid
    int refcnt;       
    struct cache_block *prev; 
    struct cache_block *next;
    pthread_rwlock_t lock;
} cache_block_t;

typedef struct cache_mgr {
    cache_block_t blocks[NBUF];
    vdev_t *backend;
    vdev_t vdev;
    cache_block_t *lru_head;
    cache_block_t *lru_tail;
    pthread_mutex_t lock;
} cache_mgr_t;


// LRU support 
cache_block_t* lookup_block(cache_mgr_t *mgr, int dev_id, int block_no);
void lru_append(cache_mgr_t *mgr, cache_block_t *blk);
void lru_remove(cache_mgr_t *mgr, cache_block_t *blk);
cache_block_t *evict_block(cache_mgr_t *mgr);


// cache layer interface
void cache_init(cache_mgr_t *mgr, vdev_t *dev, int dev_it);
void cache_destroy(cache_mgr_t *mgr);
int cache_sync(void *priv);

int cread(void *priv, void *buf, unsigned int count);
int cwrite(void *priv, const void *buf, unsigned int count);

