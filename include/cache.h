#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include "defs.h"
#include "disk.h"
/*
cache use lru, the max cache block be defined in CACHE_SIZE
*/

// cache buffer layer
#define DIRTY 1
#define VALID 2
#define CACHE_SIZE 64 // size of cache block  

typedef struct cache_block {
    size_t dev_id;    // device id 
    size_t block_no;  // block num in img, offset = block_no * block_size
    char data[BSIZE]; // block data
    uint8_t type;     // have two type dirty and valid
    int refcnt;       
    struct cache_block *prev; 
    struct cache_block *next;
    pthread_mutex_t lock;
} cache_block_t;

typedef struct cache_mgr {
    cache_block_t blocks[NBUF];
    cache_block_t *lru_head;
    cache_block_t *lru_tail;
    pthread_mutex_t lock;
} cache_mgr_t;

extern cache_mgr_t cache_mgr;

// LRU support 
cache_block_t* lookup_block(cache_mgr_t*, size_t, size_t);
void lru_append(cache_mgr_t*, cache_block_t*);
void lru_remove(cache_mgr_t*, cache_block_t*);
cache_block_t *evict_block(cache_mgr_t *);


// cache layer interface
void cache_init(cache_mgr_t*);
void cache_destroy(cache_mgr_t*);
cache_block_t *cache_get(cache_mgr_t*, int dev_id, size_t block_no);
void cache_release(cache_mgr_t*, cache_block_t*);
void cache_sync(cache_mgr_t *, disk_t *);
#endif