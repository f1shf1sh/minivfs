#pragma once

#include <stdint.h>
#include "defs.h"

/* Public API: one mounted image; each operation is serialized internally. */
enum { MY_TYPE_FILE = 1, MY_TYPE_DIR = 2 };
typedef struct { uint32_t inum, type, size; } my_stat_t;
typedef struct {
    uint32_t inum, type, size;
    char name[FILENAME_MAX_LEN];
} my_dirent_t;
typedef struct {
    uint32_t block_size, total_blocks, data_blocks, free_blocks;
    uint32_t total_inodes, free_inodes, max_file_size;
} my_fsinfo_t;

int my_mount(const char *image, const char *target);
int my_umount(const char *target);
int my_sync(void);
int my_open(const char *pathname, int flags);
int my_read(int fd, void *buf, uint32_t size);
int my_write(int fd, const void *buf, uint32_t size);
int my_close(int fd);
int my_unlink(const char *path);
int my_mkdir(const char *path);
int my_rmdir(const char *path);
int my_stat(const char *path, my_stat_t *out);
/* Start with *offset=0. Returns 1 for an entry, 0 at EOF, or -1 on error. */
int my_readdir(const char *path, uint32_t *offset, my_dirent_t *out);
int my_statfs(my_fsinfo_t *out);
