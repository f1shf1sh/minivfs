#pragma once

#include "fs/fs.h"

/* Successful resolution returns one owned inode reference. */
int path_resolve(fs_t *fs, const char *path, int create, icache_t **out);
int path_parent(fs_t *fs, const char *path, icache_t **parent, char *name);
int path_create(fs_t *fs, icache_t *parent, const char *name,
                uint32_t type, uint32_t *inum);
