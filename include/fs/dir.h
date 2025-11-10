#pragma once 

#include "fs/fs.h"
#include "fs/inode.h"

/* 
==================================================================
                        dir layer
==================================================================
*/

#define DIR_SIZE 64
#define DIR_ENTRIES_PER_BLOCK (BSIZE / DIR_SIZE)


// sizeof(dirent_t) = 64
typedef struct dirent {
    uint32_t inum;
    char name[FILENAME_MAX_LEN];
}dirent_t;

// directory interface
int dir_lookup(fs_t *fs, icache_t *dir, const char *name);
int dir_add(fs_t *fs, icache_t *dir, const char *name, uint32_t inum);
int dir_remove(fs_t *fs, icache_t *dir, const char *name);

// debug handler
int dir_list(fs_t *fs, icache_t *dir_ino);