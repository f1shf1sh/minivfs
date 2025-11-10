#include <stdlib.h>
#include "syscall.h"
#include "fs/path.h"
#include "fs/fs.h"
#include "vdev/disk.h"

// 打开文件
int sys_open(fs_t *fs, fd_t *fd_table, const char *pathname, int flags) {
    if (!pathname) 
        return -1;

    icache_t *ic = NULL;
    int create = (flags & O_CREAT) ? 1 : 0;

    if (path_resolve(&fs, pathname, create, &ic) < 0) 
        return -1;

    // 分配 fd
    int fd = -1;
    for (int i = 0; i < MAXFD; i++) {
        if (!fd_table[i].used) {
            fd = i;
            fd_table[i].used = 1;
            fd_table[i].ic = ic;
            fd_table[i].flags = flags;

            // 如果是O_APPEND，初始化offset到文件末尾
            if (flags & O_APPEND) {
                fd_table[i].offset = ic->inode.size;
            } else {
                fd_table[i].offset = 0;
            }
            break;
        }
    }
   

    return fd;
}

// 读取文件
int sys_read(fs_t *fs, fd_t *fd_table, int fd, void *buf, uint32_t size) {
    if (fd < 0 || fd >= MAXFD || !fd_table[fd].used || !buf) 
        return -1;

    fd_t *f = &fd_table[fd];
    if (!f->used || (f->flags & O_WRONLY))
        return -1;  // 写-only 文件不能读

    // 计算要读的位置
    uint32_t read_offset = f->offset;
    int ret = bread(&fs, f->ic, buf, size, 0);
    if (ret >= 0) 
        f->offset += ret;
    return ret;
}

// 写入文件
int sys_write(fs_t *fs, fd_t *fd_table, int fd, const void *buf, uint32_t size) {
    if (fd < 0 || fd >= MAXFD || !fd_table[fd].used || !buf) 
        return -1;

    fd_t *f = &fd_table[fd];

    if (!f->used || (f->flags & O_RDONLY))
        return -1; // 只读文件不能写

    // 如果 O_APPEND，移动 offset 到文件末尾
    if (f->flags & O_APPEND)
        f->offset = f->ic->inode.size;

    int ret = bwrite(&fs, f->ic, buf, size, 0);
    if (ret >= 0) 
        f->offset = ret;
    return ret;
}

// 关闭文件
int sys_close(fs_t *fs, fd_t *fd_table, int fd) {
    if (fd < 0 || fd >= MAXFD || !fd_table[fd].used) 
        return -1;

    fd_t *f = &fd_table[fd];
    iput(&fs, &f->ic);
    f->used = 0;
    return 0;
}

int sys_mount(fs_t *fs, const char *image, const char *target) {
    // 尝试加载磁盘镜像文件（比如 disk.img）
    disk_t *disk = malloc(sizeof(disk_t));
    int r = disk_init(disk, image, BSIZE, BCOUNTS);
    if (r < 0)
        return -1;

    // 调用 fs 层的 mount 函数
    return fs_mount(fs, &disk->vdev, ICACHE_SIZE);
}

int sys_umount(fs_t *fs, const char *target) {
    return fs_unmount(fs);
}