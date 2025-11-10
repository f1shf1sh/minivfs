#pragma once

#include <stdint.h>
#include "fs/fs.h"
#include "syscall.h"
#include "defs.h"

typedef struct user_context {
    fs_t fs;             // 文件系统上下文
    fd_t fd_table[MAXFD]; // 文件描述符表
} ctx_t;

extern ctx_t g_ctx;

static inline ctx_t* get_ctx() {
    return &g_ctx;
}

int my_open(const char *pathname, int flags);
int my_read(int fd, void *buf, uint32_t size);
int my_write(int fd, const void *buf, uint32_t size);
int my_close(int fd);
int my_mount(const char *image, const char *target);
int my_umount(const char *target);