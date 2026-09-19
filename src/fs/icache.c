#include "fs/fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int reclaim_unlinked(fs_t *fs, icache_t *ic) {
    if (itruncate(fs, ic) < 0 || ifree(fs, ic->inum) < 0)
        return -1;
    memset(ic, 0, sizeof(*ic));
    return 0;
}

int iget(fs_t *fs, uint32_t inum, icache_t **out) {
    if (inum == 0 || inum >= fs->sb.inode_blocks * INODES_PER_BLOCK ||
        !BIT_TST(fs->inode_bitmap, inum)) {
        errno = ENOENT;
        return -1;
    }

    icache_t *empty = NULL;
    icache_t *idle = NULL;
    for (uint32_t i = 0; i < ICACHE_SIZE; i++) {
        icache_t *slot = &fs->cache_mgr.slots[i];
        if (slot->inum == inum) {
            slot->refcnt++;
            *out = slot;
            return 0;
        }
        if (slot->inum == 0 && empty == NULL)
            empty = slot;
        else if (slot->refcnt == 0 && idle == NULL)
            idle = slot;
    }
    icache_t *slot = empty != NULL ? empty : idle;
    if (slot == NULL) {
        errno = ENFILE;
        return -1;
    }
    if (slot->inum != 0) {
        if (slot->inode.links == 0) {
            if (reclaim_unlinked(fs, slot) < 0)
                return -1;
        } else if (slot->dirty && iwrite(fs, slot->inum, slot) < 0) {
            return -1;
        }
    }

    icache_t loaded = {.inum = inum, .refcnt = 1};
    if (iread(fs, inum, &loaded) < 0)
        return -1;
    free(slot->indirect);
    *slot = loaded;
    *out = slot;
    return 0;
}

int iput(fs_t *fs, icache_t *ic) {
    if (ic->refcnt <= 0) {
        errno = EINVAL;
        return -1;
    }
    ic->refcnt--;
    if (ic->refcnt == 0 && ic->inode.links == 0)
        return reclaim_unlinked(fs, ic);
    return 0;
}

int icache_sync(fs_t *fs) {
    for (uint32_t i = 0; i < ICACHE_SIZE; i++) {
        icache_t *slot = &fs->cache_mgr.slots[i];
        if (slot->inum == 0)
            continue;
        /* A failed final close leaves a zero-reference inode to retry. */
        if (slot->refcnt == 0 && slot->inode.links == 0) {
            if (reclaim_unlinked(fs, slot) < 0)
                return -1;
        } else if (slot->dirty && iwrite(fs, slot->inum, slot) < 0) {
            return -1;
        }
    }
    return 0;
}
