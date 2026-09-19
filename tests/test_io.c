#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs/fs.h"
#include "vdev/disk.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "test_io:%d: %s\n", __LINE__, #expr); \
        goto cleanup; \
    } \
} while (0)

typedef struct {
    disk_t *disk;
    int reads_left, writes_left, fail_sync;
} faults_t;

static int fail_after(int *left) {
    if (*left < 0) return 0;
    if (*left > 0) { --*left; return 0; }
    errno = EIO;
    return 1;
}

static int fault_read(void *priv, void *buf, unsigned int block) {
    faults_t *faults = priv;
    return fail_after(&faults->reads_left) ? -1 : disk_read(faults->disk, buf, block);
}

static int fault_write(void *priv, const void *buf, unsigned int block) {
    faults_t *faults = priv;
    return fail_after(&faults->writes_left) ? -1 : disk_write(faults->disk, buf, block);
}

static int fault_sync(void *priv) {
    faults_t *faults = priv;
    if (faults->fail_sync) { errno = EIO; return -1; }
    return disk_sync(faults->disk);
}

static unsigned used_blocks(fs_t *fs) {
    unsigned used = 0;
    for (uint32_t i = 0; i < fs->sb.data_blocks; i++)
        used += BIT_TST(fs->data_bitmap, i);
    return used;
}

static int test_device(void) {
    int result = 1, fd = -1;
    char path[] = "/tmp/minivfs-io-XXXXXX";
    disk_t *disk = NULL;
    uint8_t buf[BSIZE] = {0};
    fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(ftruncate(fd, 2 * BSIZE) == 0);
    CHECK(close(fd) == 0);
    fd = -1;
    disk = malloc(sizeof(*disk));
    CHECK(disk != NULL);
    CHECK(disk_init(disk, path, BSIZE, 0) == 0);
    CHECK(disk->total_blocks == 2);
    CHECK(disk_write(disk, buf, 1) == 0);
    CHECK(disk_read(disk, buf, 2) == -1 && errno == EINVAL);
    CHECK(disk_write(disk, buf, 2) == -1 && errno == EINVAL);
    /* The first pread gets three bytes, then EOF must fail the block read. */
    CHECK(ftruncate(disk->fd, BSIZE + 3) == 0);
    CHECK(disk_read(disk, buf, 1) == -1 && errno == EIO);
    int owned_fd = disk->fd;
    disk_destroy(disk);
    disk = NULL;
    CHECK(fcntl(owned_fd, F_GETFD) == -1 && errno == EBADF);
    result = 0;
cleanup:
    if (disk) disk_destroy(disk);
    if (fd >= 0) close(fd);
    unlink(path);
    return result;
}

static int test_filesystem(const char *path) {
    int result = 1, mounted = 0, unreferenced_allocation = 0;
    disk_t *disk = NULL;
    fs_t fs = {0};
    icache_t *ic = NULL;
    faults_t faults = {.reads_left = -1, .writes_left = -1};
    uint8_t input[2 * BSIZE], output[2 * BSIZE];
    memset(input, 0x5a, sizeof(input));
    disk = malloc(sizeof(*disk));
    CHECK(disk != NULL);
    CHECK(disk_init(disk, path, BSIZE, 0) == 0);
    faults.disk = disk;
    vdev_t dev = disk->vdev;
    dev.priv = &faults;
    dev.ops = (vdev_ops_t){fault_read, fault_write, fault_sync};
    faults.reads_left = 0;
    CHECK(fs_mount(&fs, &dev, ICACHE_SIZE) == -1 && errno == EIO);
    faults.reads_left = -1;
    CHECK(fs_mount(&fs, &dev, ICACHE_SIZE) == 0);
    mounted = 1;
    unsigned before = used_blocks(&fs);
    int inum = ialloc(&fs);
    CHECK(inum > 0);
    unreferenced_allocation = inum;
    CHECK(iget(&fs, (uint32_t)inum, &ic) == 0);
    unreferenced_allocation = 0;
    ic->inode.type = TYPE_FILE;
    ic->inode.links = 1;
    ic->dirty = 1;

    faults.writes_left = 0;
    CHECK(bwrite(&fs, ic, input, sizeof(input), 0, 1) == -1 && errno == EIO);
    CHECK(ic->inode.size == 0 && used_blocks(&fs) == before);
    /* First block commits; the second block's data write fails after allocation. */
    faults.writes_left = 3;
    CHECK(bwrite(&fs, ic, input, sizeof(input), 0, 1) == BSIZE);
    CHECK(ic->inode.size == BSIZE);
    faults.writes_left = -1;
    CHECK(bwrite(&fs, ic, input + BSIZE, BSIZE, BSIZE, 1) == BSIZE);
    CHECK(ic->inode.size == sizeof(input));

    faults.reads_left = 0;
    CHECK(bread(&fs, ic, output, sizeof(output), 0) == -1 && errno == EIO);
    CHECK(bwrite(&fs, ic, input, 1, 0, 1) == -1 && errno == EIO);
    CHECK(ic->inode.size == sizeof(input));
    memset(output, 0xa5, sizeof(output));
    faults.reads_left = 1;
    CHECK(bread(&fs, ic, output, sizeof(output), 0) == BSIZE);
    CHECK(!memcmp(input, output, BSIZE) && output[BSIZE] == 0xa5);
    faults.reads_left = -1;
    CHECK(bread(&fs, ic, output, sizeof(output), 0) == (int)sizeof(output));
    CHECK(!memcmp(input, output, sizeof(input)));

    faults.writes_left = 0;
    CHECK(fs_sync(&fs) == -1 && errno == EIO && ic->dirty);
    faults.writes_left = -1;
    faults.fail_sync = 1;
    CHECK(fs_sync(&fs) == -1 && errno == EIO);
    faults.fail_sync = 0;
    CHECK(fs_sync(&fs) == 0);

    ic->inode.links = 0;
    ic->dirty = 1;
    faults.writes_left = 0;
    int closed = iput(&fs, ic);
    ic = NULL; /* iput consumes the reference even when reclamation fails. */
    CHECK(closed == -1 && errno == EIO);
    CHECK(BIT_TST(fs.inode_bitmap, (uint32_t)inum));
    faults.writes_left = -1;
    CHECK(fs_sync(&fs) == 0);
    CHECK(!BIT_TST(fs.inode_bitmap, (uint32_t)inum));
    CHECK(used_blocks(&fs) == before);
    result = 0;
cleanup:
    faults.reads_left = faults.writes_left = -1;
    faults.fail_sync = 0;
    if (mounted) {
        if (ic) {
            ic->inode.links = 0;
            ic->dirty = 1;
            if (iput(&fs, ic) < 0) result = 1;
        }
        if (unreferenced_allocation)
            ifree(&fs, (uint32_t)unreferenced_allocation);
        if (fs_unmount(&fs) < 0) result = 1;
    }
    if (disk) disk_destroy(disk);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <image>\n", argv[0]);
        return 1;
    }
    if (test_device() || test_filesystem(argv[1])) return 1;
    puts("test_io: PASS");
    return 0;
}
