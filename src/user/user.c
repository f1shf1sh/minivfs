#include "user.h"
#include "syscall.h"

ctx_t g_ctx;

int my_open(const char *pathname, int flags) {
    ctx_t *ctx = get_ctx();  // 获取全局 ctx

    return sys_open(&ctx->fs, ctx->fd_table, pathname, flags);
}

int my_read(int fd, void *buf, uint32_t size) {
    ctx_t *ctx = get_ctx();  
    return sys_read(&ctx->fs, ctx->fd_table, fd, buf, size);
}

int my_write(int fd, const void *buf, uint32_t size) {
    ctx_t *ctx = get_ctx(); 
    return sys_write(&ctx->fs, ctx->fd_table, fd, buf, size);
}

int my_close(int fd) {
    ctx_t *ctx = get_ctx(); 
    return sys_close(&ctx->fs, ctx->fd_table, fd);
}

int my_mount(const char *image, const char *target) {
    ctx_t *ctx = get_ctx();
    return sys_mount(&ctx->fs, image, target);
}

int my_umount(const char *target) {
    ctx_t *ctx = get_ctx();
    return sys_umount(&ctx->fs, target);
}