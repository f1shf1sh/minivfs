#ifndef BLK_H
#define BLK_H

#include <stdint.h>
#include "defs.h"
#include "cache.h"

#define BIT_SET(map, n)   ((map)[(n)/8] |=  (1u << ((n) % 8)))
#define BIT_CLR(map, n)   ((map)[(n)/8] &= ~(1u << ((n) % 8)))
#define BIT_TST(map, n)   (((map)[(n)/8] >> ((n) % 8)) & 1u)


typedef struct blk_mgr {
    vdev_t *backend; // may point to disk.vdev or cache_mgr.vdev
    unsigned char *bitmap;
    unsigned int total_blocks;
    pthread_mutex_t lock;
} blk_mgr_t;

void blk_init(blk_mgr_t *m, vdev_t *backend, int total_blocks);
void blk_destroy(blk_mgr_t *m);

// bitmap interface
int blk_alloc(blk_mgr_t *mgr);
void blk_free(blk_mgr_t *mgr, unsigned int block_no);

// simple read/write wrappers that use backend->ops
int blk_read(void *priv, void *buf, unsigned int block_no);
int blk_write(void *priv, const void *buf, unsigned int block_no);
int blk_sync(void *priv);

#endif