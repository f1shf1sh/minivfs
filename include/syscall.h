#pragma once

#include "fs/inode.h"

typedef struct file {
    icache_t *ic;
    uint32_t offset;
    int flags;
    int used;            // 是否在用
} fd_t;

int sys_open(fs_t *fs, fd_t *fd_table, const char *path, int flags);
int sys_read(fs_t *fs, fd_t *fd_table, int fd, void *buf, uint32_t count);
int sys_write(fs_t *fs, fd_t *fd_table, int fd, const void *buf, uint32_t count);
int sys_close(fs_t *fs, fd_t *fd_table, int fd);
int sys_mount(fs_t *fs, const char *image, const char *target);
int sys_umount(fs_t *fs, const char *target);