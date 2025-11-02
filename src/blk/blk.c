#include "blk.h"
#include <string.h>
#include <stdlib.h>

/* initialize block layer with a backend vdev (could be disk.vdev or cache_mgr.vdev) */
void blk_init(blk_mgr_t *mgr, vdev_t *backend, int total_blocks) {
    mgr->backend = backend;
    mgr->total_blocks = total_blocks;
    mgr->bitmap = (unsigned char*)calloc((total_blocks + 7) / 8, sizeof(unsigned char));
    if (!mgr->bitmap) {
        free(mgr);
    }
    pthread_mutex_init(&mgr->lock, NULL);
}

void blk_destroy(blk_mgr_t *mgr) {
    if (!mgr) 
        return;
    free(mgr->bitmap);
    pthread_mutex_destroy(&mgr->lock);
    free(mgr);
}

int blk_read(void *priv, void *buf, unsigned int block_no) {
    blk_mgr_t *mgr = (blk_mgr_t*)priv;
    if (!mgr || !buf || block_no >= mgr->total_blocks) 
        return -1;
    if (!mgr->backend) 
        return -1;

    return mgr->backend->ops.read(mgr->backend->priv, buf, block_no);
}

int blk_write(void *priv, const void *buf, unsigned int block_no) {
    blk_mgr_t *mgr = (blk_mgr_t*)priv;
    if (!mgr || !buf || block_no >= mgr->total_blocks) 
        return -1;
    if (!mgr->backend) 
        return -1;

    return mgr->backend->ops.write(mgr->backend->priv, buf, block_no);
}

int blk_sync(void *priv) {
    blk_mgr_t *mgr = (blk_mgr_t*)priv;
    return (mgr->backend->ops).sync((void*)mgr->backend->priv);
}

// ---------------- 分配/释放块 ----------------
int blk_alloc(blk_mgr_t *mgr) {
    int idx = -1;
    pthread_mutex_lock(&mgr->lock);
    for (size_t i = 0; i < mgr->total_blocks; i++) {
        if (!BIT_TST(mgr->bitmap, i)) {
            BIT_SET(mgr->bitmap, i);
            idx = i;
            break;
        }
    }
    pthread_mutex_unlock(&mgr->lock);
    return idx; // 无空闲块
}

void blk_free(blk_mgr_t *mgr, unsigned int block_no) {
    if (block_no >= mgr->total_blocks) 
        return;
    pthread_mutex_lock(&mgr->lock);
    BIT_CLR(mgr->bitmap, block_no);
    pthread_mutex_unlock(&mgr->lock);
}

