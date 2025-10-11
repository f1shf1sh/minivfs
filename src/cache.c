#include "../include/cache.h"
#include "../include/disk.h"
#include "../include/defs.h"
#include <stdlib.h>

cache_mgr_t cache_mgr;

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

cache_block_t* lookup_block(cache_mgr_t *mgr, size_t dev_id, size_t block_no) {
    if(!mgr)
        return NULL;

    for (int i = 0; i < NBUF; i++) {
        cache_block_t *blk = &mgr->blocks[i];
        if ((blk->type & VALID) && blk->dev_id == dev_id && blk->block_no == block_no)
            return blk;
    }
    return NULL;
}

void cache_init(cache_mgr_t *mgr) {
    pthread_mutex_init(&mgr->lock, NULL);
    mgr->lru_head = mgr->lru_tail = NULL;

    for (int i = 0; i < CACHE_SIZE; i++) {
        cache_block_t *blk = &mgr->blocks[i];
        blk->dev_id = -1;
        blk->type = 0;
        blk->block_no = 0;
        blk->refcnt = 0;
        pthread_mutex_init(&blk->lock, NULL);
        lru_append(mgr, blk);
    }
}

void cache_destroy(cache_mgr_t *mgr) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache_block_t *blk = &mgr->blocks[i];
        free(blk->data);
        pthread_mutex_destroy(&blk->lock);
    }
    pthread_mutex_destroy(&mgr->lock);
}

cache_block_t *cache_get(cache_mgr_t *mgr, int dev_id, size_t block_no) {
    pthread_mutex_lock(&mgr->lock);

    // 查看cache是否命中，如果命中，将blk添加到lru尾部
    cache_block_t *blk = lookup_block(mgr, dev_id, block_no);
    if (blk) {
        blk->refcnt++;
        lru_remove(mgr, blk);
        lru_append(mgr, blk);
        pthread_mutex_unlock(&mgr->lock);
        return blk;
    }

    // cache没有命中，块置换
    blk = evict_block(mgr);
    if (!blk) {
        return NULL; //  缓存全部被占用
    }
    pthread_mutex_lock(&blk->lock);
    if (blk->type&DIRTY) { // 如果块被修改
        struct disk *disk = disk_get(&disk_mgr, blk->dev_id);
        if (disk) { // 找到块对应的磁盘, 写入数据
            disk->rws->write(disk, (const void*)blk->data, BSIZE, BSIZE*block_no);
        }
    }
    blk->dev_id = dev_id;
    blk->block_no = block_no;
    blk->type = VALID;
    blk->refcnt = 1;

    struct disk *disk = disk_get(&disk_mgr, dev_id); // 获取当前对应dev_id的设备
    if (disk) {
        disk->rws->read(disk, blk->data, BSIZE, block_no*BSIZE);
        lru_append(mgr, blk);
    }
    pthread_mutex_unlock(&blk->lock);
    pthread_mutex_unlock(&mgr->lock);
    return blk;
}

void cache_release(cache_mgr_t *mgr, cache_block_t *blk) {
    pthread_mutex_lock(&mgr->lock);
    blk->refcnt--;
    if (blk->refcnt < 0)
        blk->refcnt = 0;
    if (blk->refcnt == 0) {
        lru_remove(mgr, blk);
        if (blk->type == DIRTY) {
            struct disk *disk = disk_get(&disk_mgr, blk->dev_id);
            disk->rws->write(disk, blk->data, BSIZE, BSIZE*blk->block_no);
            blk->type = 0;
        }
    }
    pthread_mutex_unlock(&mgr->lock);
}

void cache_sync(cache_mgr_t *mgr, struct disk *disk) {
    pthread_mutex_lock(&mgr->lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache_block_t *blk = &mgr->blocks[i];
        pthread_mutex_lock(&blk->lock);
        if (blk->type == DIRTY) {
            disk_write(disk, blk->data, BSIZE, BSIZE*blk->block_no);
            blk->type = 0;
        }
        pthread_mutex_unlock(&blk->lock);
    }
    pthread_mutex_unlock(&mgr->lock);
}