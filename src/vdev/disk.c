#include "vdev/disk.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int disk_read(void *priv, void *buf, unsigned int block_no) {
    disk_t *disk = priv;
    if (block_no >= disk->total_blocks) {
        errno = EINVAL;
        return -1;
    }

    size_t done = 0;
    off_t offset = (off_t)block_no * disk->block_size;
    while (done < disk->block_size) {
        ssize_t count = pread(disk->fd, (char *)buf + done,
                              disk->block_size - done, offset + (off_t)done);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            return -1;
        }
        done += (size_t)count;
    }
    return 0;
}

int disk_write(void *priv, const void *buf, unsigned int block_no) {
    disk_t *disk = priv;
    if (block_no >= disk->total_blocks) {
        errno = EINVAL;
        return -1;
    }

    size_t done = 0;
    off_t offset = (off_t)block_no * disk->block_size;
    while (done < disk->block_size) {
        ssize_t count = pwrite(disk->fd, (const char *)buf + done,
                               disk->block_size - done, offset + (off_t)done);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            return -1;
        }
        done += (size_t)count;
    }
    return 0;
}

int disk_sync(void *priv) {
    disk_t *disk = priv;
    int result;
    do {
        result = fsync(disk->fd);
    } while (result < 0 && errno == EINTR);
    return result;
}

int disk_init(disk_t *disk, const char *path, unsigned int block_size,
              unsigned int total_blocks) {
    memset(disk, 0, sizeof(*disk));
    disk->fd = -1;
    if (block_size == 0) {
        errno = EINVAL;
        return -1;
    }

    int fd = open(path, O_RDWR);
    if (fd < 0)
        return -1;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    if (!S_ISREG(st.st_mode) || st.st_size <= 0 ||
        st.st_size % block_size != 0 ||
        (uint64_t)st.st_size / block_size > UINT_MAX ||
        (total_blocks != 0 && (uint64_t)st.st_size / block_size != total_blocks)) {
        close(fd);
        errno = EINVAL;
        return -1;
    }

    disk->fd = fd;
    disk->block_size = block_size;
    disk->total_blocks = (unsigned int)(st.st_size / block_size);
    disk->vdev.ops.read = disk_read;
    disk->vdev.ops.write = disk_write;
    disk->vdev.ops.sync = disk_sync;
    disk->vdev.dev_id = (unsigned int)fd;
    disk->vdev.priv = disk;
    snprintf(disk->vdev.name, sizeof(disk->vdev.name), "%s", path);
    return 0;
}

void disk_destroy(disk_t *disk) {
    if (disk == NULL)
        return;
    if (disk->fd >= 0)
        close(disk->fd);
    free(disk);
}
