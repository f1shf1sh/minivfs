#include "vdev/cache.h"
#include "vdev/disk.h"
#include "defs.h"
#include "vdev/vdev.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 获取块
cache_block_t *cache_get(cache_mgr_t *mgr, int block_no, int write) {
    pthread_mutex_lock(&mgr->lock);

    cache_block_t *blk = NULL;
    // 查找已存在块
    for (int i = 0; i < NBUF; i++) {
        if ((mgr->blocks[i].flags & VALID) &&
            mgr->blocks[i].block_no == block_no) {
            // printf("[+] cache hit block_no:%d\n", block_no);
            blk = &mgr->blocks[i];
            blk->refcnt++;
            if (blk != mgr->lru_tail) 
                lru_remove(mgr, blk);
            pthread_mutex_unlock(&mgr->lock);

            if (write)
                pthread_rwlock_wrlock(&blk->lock);
            else
                pthread_rwlock_rdlock(&blk->lock);
            return blk;
        }
    }

    // 没找到，挑空块或淘汰
    for (int i = 0; i < NBUF; i++) {
        if (!(mgr->blocks[i].flags & VALID)) {
            blk = &mgr->blocks[i];
            break;
        }
    }

    // 没有找到空块，块淘汰
    if (!blk) { // 淘汰 LRU
        blk = mgr->lru_head;
        while (blk && blk->refcnt > 0)
            blk = blk->next;
        if (!blk) { 
            pthread_mutex_unlock(&mgr->lock); 
            return NULL; 
        }

        if (blk->flags & DIRTY)
            mgr->backend->ops.write(mgr->backend->priv, blk->data, blk->block_no);

        blk->flags = 0;
    }

    // 读取数据
    if (mgr->backend)
        mgr->backend->ops.read(mgr->backend->priv, blk->data, block_no);
    else
        memset(blk->data, 0, BSIZE);

    blk->block_no = block_no;
    blk->flags = VALID;
    blk->refcnt = 1;

    lru_append(mgr, blk);
    pthread_mutex_unlock(&mgr->lock);

    if (write)
        pthread_rwlock_wrlock(&blk->lock);
    else
        pthread_rwlock_rdlock(&blk->lock);

    return blk;
}

// 释放块
void cache_put(cache_mgr_t *mgr, cache_block_t *blk) {
    pthread_rwlock_unlock(&blk->lock);
    pthread_mutex_lock(&mgr->lock);
    blk->refcnt--;
    pthread_mutex_unlock(&mgr->lock);
}

int cache_read(void *priv, void *buf, unsigned int block_no) {
    cache_mgr_t *mgr = (cache_mgr_t*)priv;
    cache_block_t *blk = cache_get(mgr, block_no, 0);
    if (!blk) 
        return -1;
    memcpy(buf, blk->data, BSIZE);
    cache_put(mgr, blk);
    return 0;
}

int cache_write(void *priv, const void *buf, unsigned int block_no) {
    cache_mgr_t *mgr = (cache_mgr_t*)priv;
    cache_block_t *blk = cache_get(mgr,block_no, 1);
    if (!blk) 
        return -1;
    memcpy(blk->data, buf, BSIZE);
    blk->flags |= DIRTY;
    cache_put(mgr, blk);
    return 0;
}

int cache_sync(void *priv) {
    cache_mgr_t *mgr = (cache_mgr_t*)priv;

    if (!mgr) 
        return 0;
    if (!mgr->backend)
        return 0; 
    pthread_mutex_lock(&mgr->lock);
    for (int i = 0; i < NBUF; ++i) {
        cache_block_t *b = &mgr->blocks[i];
        pthread_rwlock_wrlock(&b->lock);
        if ((b->flags & DIRTY) && (b->flags & VALID)) {
            mgr->backend->ops.write(mgr->backend->priv, b->data, b->block_no);
            b->flags &= ~DIRTY;
        }
        pthread_rwlock_unlock(&b->lock);
    }
    pthread_mutex_unlock(&mgr->lock);
    return 0;

}

// move blk to lru tail
void lru_append(cache_mgr_t *mgr, cache_block_t *blk) {
    blk->next = NULL;
    blk->prev = mgr->lru_tail;

    if (mgr->lru_tail)
        mgr->lru_tail->next = blk;
    else
        mgr->lru_head = blk;
    mgr->lru_tail = blk;
}

void lru_remove(cache_mgr_t *mgr, cache_block_t *blk) {
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

cache_block_t *evict_block(cache_mgr_t *mgr) {
    cache_block_t *blk = mgr->lru_head;
    while (blk) {
        if (blk->refcnt == 0) {
            lru_remove(mgr, blk);
            lru_append(mgr, blk);
            return blk;
        }
        blk = blk->next;
    }
    return NULL;
}

cache_block_t* lookup_block(cache_mgr_t *mgr, int dev_id, int block_no) {
    if(!mgr)
        return NULL;

    for (int i = 0; i < NBUF; i++) {
        cache_block_t *blk = &mgr->blocks[i];
        if ((blk->flags & VALID) && blk->dev_id == dev_id && blk->block_no == block_no)
            return blk;
    }
    return NULL;
}

void cache_init(cache_mgr_t *mgr, vdev_t *backend, int dev_id) {
    if (!mgr) 
        return;
    memset(mgr, 0, sizeof(cache_mgr_t));


    for (int i = 0; i < NBUF; ++i) {
        cache_block_t *b = &mgr->blocks[i];
        b->dev_id = -1;
        b->block_no = -1;
        b->flags = 0;
        b->refcnt = 0;
        b->prev = b->next = NULL;
        memset(b->data, 0, BSIZE);
        pthread_rwlock_init(&b->lock, NULL);
    }

    /*

        struct vdev {
            char name[32];             // 设备名
            unsigned int dev_id;       // 唯一ID
            void *priv;                // 私有上下文 (disk结构或其他)
            vdev_ops_t ops;            // 操作函数表
        };

        typedef struct cache_mgr {
            cache_block_t blocks[NBUF];
            cache_block_t *lru_head;
            cache_block_t *lru_tail;

            vdev_t *backend;
            vdev_t vdev;
            pthread_mutex_t lock;
        } cache_mgr_t;

    */

    mgr->backend = backend;
    mgr->vdev.dev_id = dev_id;
    mgr->vdev.priv = (void*)mgr;
    strcpy(mgr->vdev.name, "cache");
    mgr->vdev.ops.read = cache_read;
    mgr->vdev.ops.write = cache_write;
    mgr->vdev.ops.sync = cache_sync;

    mgr->lru_head = mgr->lru_tail = NULL;
    pthread_mutex_init(&mgr->lock, NULL);

}

void cache_destroy(cache_mgr_t *mgr) {
    if (!mgr) 
        return;

    /* write back dirty */
    cache_sync(mgr);

    // free other source
    pthread_mutex_destroy(&mgr->lock);
    for (int i = 0; i < NBUF; ++i) {
        pthread_rwlock_destroy(&mgr->blocks[i].lock);
    }
}

