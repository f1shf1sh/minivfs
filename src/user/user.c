#include <pthread.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

/* One owner for the mount, descriptor table, and public-operation lock. */
static struct {
    pthread_mutex_t lock;
    int mounted;
    fs_t fs;
    fd_t table[MAXFD];
} context = {.lock = PTHREAD_MUTEX_INITIALIZER};

int my_mount(const char *image, const char *target) {
    pthread_mutex_lock(&context.lock);
    int r;
    if (context.mounted) { errno = EBUSY; r = -1; }
    else r = sys_mount(&context.fs, image, target);
    if (!r) {
        memset(context.table, 0, sizeof(context.table));
        context.mounted = 1;
    }
    pthread_mutex_unlock(&context.lock);
    return r;
}

int my_umount(const char *target) {
    pthread_mutex_lock(&context.lock);
    int r = -1;
    errno = context.mounted ? EINVAL : ENODEV;
    if (context.mounted && target && !strcmp(target, "/")) {
        r = 0;
        for (int fd=1; fd<MAXFD; fd++)
            if (context.table[fd].used &&
                sys_close(&context.fs, context.table, fd) < 0) r = -1;
        if (!r) r = sys_umount(&context.fs, target);
        if (!r) context.mounted = 0;
    }
    pthread_mutex_unlock(&context.lock);
    return r;
}

/* This single lock also covers append offsets and namespace read/modify/write. */
#define API_CALL(expression) \
    do { \
        pthread_mutex_lock(&context.lock); \
        int result; \
        if (context.mounted) result = (expression); \
        else { errno = ENODEV; result = -1; } \
        pthread_mutex_unlock(&context.lock); \
        return result; \
    } while (0)

int my_sync(void) { API_CALL(fs_sync(&context.fs)); }
int my_open(const char *path, int flags) {
    API_CALL(sys_open(&context.fs, context.table, path, flags));
}
int my_read(int fd, void *buf, uint32_t size) {
    API_CALL(sys_read(&context.fs, context.table, fd, buf, size));
}
int my_write(int fd, const void *buf, uint32_t size) {
    API_CALL(sys_write(&context.fs, context.table, fd, buf, size));
}
int my_close(int fd) { API_CALL(sys_close(&context.fs, context.table, fd)); }
int my_unlink(const char *path) { API_CALL(sys_unlink(&context.fs, path, 0)); }
int my_mkdir(const char *path) { API_CALL(sys_mkdir(&context.fs, path)); }
int my_rmdir(const char *path) { API_CALL(sys_unlink(&context.fs, path, 1)); }
int my_stat(const char *path, my_stat_t *out) {
    API_CALL(sys_stat(&context.fs, path, out));
}
int my_readdir(const char *path, uint32_t *offset, my_dirent_t *out) {
    API_CALL(sys_readdir(&context.fs, path, offset, out));
}
int my_statfs(my_fsinfo_t *out) { API_CALL(sys_statfs(&context.fs, out)); }
