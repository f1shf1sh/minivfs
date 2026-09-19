#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"
#include "fs/path.h"
#include "fs/dir.h"
#include "vdev/disk.h"

int sys_open(fs_t *fs, fd_t *table, const char *path, int flags) {
    int mode = flags & O_ACCMODE;
    if ((mode != O_RDONLY && mode != O_WRONLY && mode != O_RDWR) ||
        (flags & ~(O_ACCMODE | O_CREAT | O_APPEND | O_TRUNC)) ||
        ((flags & O_TRUNC) && mode == O_RDONLY)) { errno = EINVAL; return -1; }
    int fd;
    for (fd = 1; fd < MAXFD && table[fd].used; fd++) {}
    if (fd == MAXFD) { errno = EMFILE; return -1; }
    icache_t *ic;
    if (path_resolve(fs, path, !!(flags & O_CREAT), &ic) < 0) return -1;
    if (ic->inode.type != TYPE_FILE) {
        iput(fs, ic);
        errno = EISDIR;
        return -1;
    }
    if ((flags & O_TRUNC) && itruncate(fs, ic) < 0) {
        iput(fs, ic);
        return -1;
    }
    table[fd] = (fd_t){.ic = ic, .flags = flags, .used = 1, .offset = 0};
    return fd;
}

static int valid_fd(fd_t *table, int fd) {
    if (fd > 0 && fd < MAXFD && table[fd].used) return 1;
    errno = EBADF;
    return 0;
}

int sys_read(fs_t *fs, fd_t *table, int fd, void *buf, uint32_t size) {
    if (!valid_fd(table, fd)) return -1;
    if (!buf && size) { errno = EINVAL; return -1; }
    if ((table[fd].flags & O_ACCMODE) == O_WRONLY) { errno = EBADF; return -1; }
    if (!size) return 0;
    fd_t *f = &table[fd];
    int n = bread(fs, f->ic, buf, size, f->offset);
    if (n > 0) f->offset += (uint32_t)n;
    return n;
}

int sys_write(fs_t *fs, fd_t *table, int fd, const void *buf, uint32_t size) {
    if (!valid_fd(table, fd)) return -1;
    if (!buf && size) { errno = EINVAL; return -1; }
    if ((table[fd].flags & O_ACCMODE) == O_RDONLY) { errno = EBADF; return -1; }
    if (!size) return 0;
    fd_t *f = &table[fd];
    if (f->flags & O_APPEND) f->offset = f->ic->inode.size;
    int n = bwrite(fs, f->ic, buf, size, f->offset, 1);
    if (n > 0) f->offset += (uint32_t)n;
    return n;
}

int sys_close(fs_t *fs, fd_t *table, int fd) {
    if (!valid_fd(table, fd)) return -1;
    int r = iput(fs, table[fd].ic);
    memset(&table[fd], 0, sizeof(table[fd]));
    return r;
}

int sys_mount(fs_t *fs, const char *image, const char *target) {
    if (!image || !target || strcmp(target, "/")) { errno = EINVAL; return -1; }
    disk_t *disk = calloc(1, sizeof(*disk));
    if (!disk) return -1;
    if (disk_init(disk, image, BSIZE, 0) < 0) {
        free(disk);
        return -1;
    }
    if (fs_mount(fs, &disk->vdev, ICACHE_SIZE) < 0) {
        disk_destroy(disk);
        return -1;
    }
    return 0;
}

int sys_umount(fs_t *fs, const char *target) {
    if (!target || strcmp(target, "/")) { errno = EINVAL; return -1; }
    disk_t *disk = fs->vdev->priv;
    if (fs_unmount(fs) < 0) return -1;
    disk_destroy(disk);
    return 0;
}

int sys_mkdir(fs_t *fs, const char *path) {
    icache_t *parent;
    char name[FILENAME_MAX_LEN];
    uint32_t inum;
    if (path_parent(fs, path, &parent, name) < 0) return -1;
    int r = path_create(fs, parent, name, TYPE_DIR, &inum);
    int release = iput(fs, parent);
    return r < 0 ? r : release;
}

int sys_unlink(fs_t *fs, const char *path, int directory) {
    icache_t *parent, *target;
    char name[FILENAME_MAX_LEN];
    if (!path || !path[0] || (!directory && path[strlen(path)-1] == '/'))
        { errno = EINVAL; return -1; }
    if (path_parent(fs, path, &parent, name) < 0) return -1;
    int inum = dir_lookup(fs, parent, name);
    if (inum < 0 || (uint32_t)inum == fs->root_inum ||
        iget(fs, (uint32_t)inum, &target) < 0) {
        iput(fs, parent);
        return -1;
    }
    int r = -1;
    if (directory && target->inode.type != TYPE_DIR) { errno = ENOTDIR; goto done; }
    if (!directory && target->inode.type != TYPE_FILE) { errno = EISDIR; goto done; }
    if (directory) {
        int empty = dir_is_empty(fs, target);
        if (empty != 1) { if (!empty) errno = ENOTEMPTY; goto done; }
    }
    if (dir_remove(fs, parent, name) < 0) goto done;
    target->inode.links--;
    target->dirty = 1;
    r = 0;
done:
    if (iput(fs, target) < 0) r = -1;
    if (iput(fs, parent) < 0) r = -1;
    return r;
}

int sys_stat(fs_t *fs, const char *path, my_stat_t *out) {
    if (!out) { errno = EINVAL; return -1; }
    icache_t *ic;
    if (path_resolve(fs, path, 0, &ic) < 0) return -1;
    *out = (my_stat_t){.inum=ic->inum, .type=ic->inode.type, .size=ic->inode.size};
    return iput(fs, ic);
}

int sys_readdir(fs_t *fs, const char *path, uint32_t *offset, my_dirent_t *out) {
    if (!offset || !out) { errno = EINVAL; return -1; }
    icache_t *dir;
    if (path_resolve(fs, path, 0, &dir) < 0) return -1;
    dirent_t entry;
    uint32_t next = *offset;
    int r = dir_read_entry(fs, dir, &next, &entry);
    if (r > 0) {
        icache_t *ic;
        if (iget(fs, entry.inum, &ic) < 0) r = -1;
        else {
            out->inum = entry.inum;
            out->type = ic->inode.type;
            out->size = ic->inode.size;
            memcpy(out->name, entry.name, sizeof(out->name));
            if (iput(fs, ic) < 0) r = -1;
        }
    }
    if (iput(fs, dir) < 0) r = -1;
    if (r >= 0) *offset = next;
    return r;
}

int sys_statfs(fs_t *fs, my_fsinfo_t *out) {
    if (!out) { errno = EINVAL; return -1; }
    *out = (my_fsinfo_t){
        .block_size=BSIZE, .total_blocks=fs->sb.total_blocks,
        .data_blocks=fs->sb.data_blocks,
        .total_inodes=fs->sb.inode_blocks * INODES_PER_BLOCK,
        .max_file_size=MAX_FILE_SIZE};
    for (uint32_t i=0; i<out->data_blocks; i++)
        if (!BIT_TST(fs->data_bitmap, i)) out->free_blocks++;
    for (uint32_t i=0; i<out->total_inodes; i++)
        if (!BIT_TST(fs->inode_bitmap, i)) out->free_inodes++;
    return 0;
}
