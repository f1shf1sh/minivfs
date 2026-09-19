#pragma once

#include "fs/fs.h"
#include "user.h"

/* Internal API. The caller holds the public API mutex. */
typedef struct file {
    icache_t *ic;
    uint32_t offset;
    int flags;
    int used;
} fd_t;

int sys_open(fs_t *fs, fd_t *table, const char *path, int flags);
int sys_read(fs_t *fs, fd_t *table, int fd, void *buf, uint32_t size);
int sys_write(fs_t *fs, fd_t *table, int fd, const void *buf, uint32_t size);
int sys_close(fs_t *fs, fd_t *table, int fd);
int sys_mount(fs_t *fs, const char *image, const char *target);
int sys_umount(fs_t *fs, const char *target);
int sys_unlink(fs_t *fs, const char *path, int directory);
int sys_mkdir(fs_t *fs, const char *path);
int sys_stat(fs_t *fs, const char *path, my_stat_t *out);
int sys_readdir(fs_t *fs, const char *path, uint32_t *offset, my_dirent_t *out);
int sys_statfs(fs_t *fs, my_fsinfo_t *out);
