#pragma once

#include "fs/format.h"

#define ICACHE_SIZE 64

typedef struct icache {
    uint32_t inum;
    inode_t inode;
    uint32_t *indirect;
    int refcnt;
    int dirty;
} icache_t;

typedef struct icache_mgr {
    icache_t slots[ICACHE_SIZE];
} icache_mgr_t;

typedef struct fs fs_t;

/* Internal operations run under the public API's filesystem mutex. */
int balloc(fs_t *fs);
int bfree(fs_t *fs, uint32_t block);
int ialloc(fs_t *fs);
int ifree(fs_t *fs, uint32_t inum);

int iget(fs_t *fs, uint32_t inum, icache_t **out);
/* Consume one reference, including when final reclamation fails. */
int iput(fs_t *fs, icache_t *ic);
int icache_sync(fs_t *fs);

int iread(fs_t *fs, uint32_t inum, icache_t *ic);
int iwrite(fs_t *fs, uint32_t inum, icache_t *ic);
int itruncate(fs_t *fs, icache_t *ic);

int bget(fs_t *fs, icache_t *ic, uint32_t index, int allocate);
int bread(fs_t *fs, icache_t *ic, void *buffer, uint32_t size, uint32_t offset);
int bwrite(fs_t *fs, icache_t *ic, const void *buffer,
           uint32_t size, uint32_t offset, int update_size);
