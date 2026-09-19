#pragma once

#include "fs/fs.h"

/* Directory entries only; inode lifetime belongs to the namespace API. */
int dir_lookup(fs_t *fs, icache_t *dir, const char *name);
int dir_add(fs_t *fs, icache_t *dir, const char *name, uint32_t inum);
int dir_remove(fs_t *fs, icache_t *dir, const char *name);
int dir_read_entry(fs_t *fs, icache_t *dir, uint32_t *offset, dirent_t *out);
int dir_is_empty(fs_t *fs, icache_t *dir);
