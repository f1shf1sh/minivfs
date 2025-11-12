#include "vdev/cache.h"
#include "vdev/disk.h"
#include "defs.h"
#include "vdev/vdev.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 获取块
int cget(bcache_mgr_t *mgr, int block_no, bcache_t **bc) {
}

// 释放块
int cput(bcache_mgr_t *mgr, bcache_t *bc) {

}

int cache_read(void *priv, void *buf, unsigned int block_no) {

}



int cache_sync(void *priv) {
    bcache_mgr_t *mgr = (bcache_mgr_t*)priv;

    if (!mgr) 
        return 0;
    if (!mgr->backend)
        return 0;
    
    for (int i = 0; i < NBUF; ++i) {
        bcache_t *b = &mgr->blocks[i];
        if ((b->flags & DIRTY) && (b->flags & VALID)) {
            mgr->backend->ops.write(mgr->backend->priv, b->data, b->block_no);
            b->flags &= ~DIRTY;
        }
    }
    return 0;
}

// move blk to lru tail
void lru_append(bcache_mgr_t *mgr, bcache_t *blk) {
    blk->next = NULL;
    blk->prev = mgr->lru_tail;

    if (mgr->lru_tail)
        mgr->lru_tail->next = blk;
    else
        mgr->lru_head = blk;
    mgr->lru_tail = blk;
}

void lru_remove(bcache_mgr_t *mgr, bcache_t *blk) {
    if (blk->prev) 
        blk->prev->next = blk->next;
    else 
        mgr->lru_head = blk->next;

    if (blk->next) 
        blk->next->prev = blk->prev;
    else 
        mgr->lru_tail = blk->prev;

    blk->prev = blk->next = NULL;
}


int cache_init(bcache_mgr_t *mgr, vdev_t *backend) {
    if (!mgr || !backend) 
        return -1;

    memset(mgr, 0, sizeof(bcache_mgr_t));
    for (int i = 0; i < NBUF; ++i) {
        bcache_t *b = &mgr->blocks[i];
        b->block_no = -1;
        b->flags = 0;
        b->refcnt = 0;
        b->prev = b->next = NULL;
        memset(b->data, 0, BSIZE);
        lock_init(&b->lock);
    }

    mgr->backend = backend;
    mgr->vdev.priv = (void*)mgr;
    strcpy(mgr->vdev.name, "block cache");
    mgr->vdev.ops.read = cread;
    mgr->vdev.ops.write = cwrite;
    mgr->vdev.ops.sync = csync;
    mgr->lru_head = mgr->lru_tail = NULL;
}

int cache_destroy(bcache_mgr_t *mgr) {
    if (!mgr) 
        return;
    /* write back dirty */
    cache_sync(mgr);
}

