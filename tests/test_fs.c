#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "user.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s (errno=%d)\n", __FILE__, __LINE__, #condition, errno); \
        exit(1); \
    } \
} while (0)

static void put_file(const char *path, const void *data, uint32_t size) {
    int fd = my_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    CHECK(fd > 0);
    CHECK(my_write(fd, data, size) == (int)size);
    CHECK(my_close(fd) == 0);
}

static void expect_file(const char *path, const void *data, uint32_t size) {
    int fd = my_open(path, O_RDONLY);
    CHECK(fd > 0);
    unsigned char *readback = malloc(size + 1);
    CHECK(readback != NULL);
    CHECK(my_read(fd, readback, size + 1) == (int)size);
    CHECK(memcmp(readback, data, size) == 0);
    CHECK(my_read(fd, readback, 1) == 0);
    CHECK(my_close(fd) == 0);
    free(readback);
}

static void namespace_tests(void) {
    CHECK(my_mkdir("/suite") == 0);
    CHECK(my_mkdir("/suite/sub") == 0);
    CHECK(my_mkdir("/suite") == -1 && errno == EEXIST);
    put_file("/suite/file", "abcdef", 6);
    expect_file("/suite/sub/../file", "abcdef", 6);
    expect_file("//suite/./file", "abcdef", 6);
    CHECK(my_open("/suite/file/child", O_CREAT | O_RDWR) == -1 && errno == ENOTDIR);
    CHECK(my_open("/suite/file/", O_RDONLY) == -1 && errno == ENOTDIR);
    CHECK(my_open("/missing/../suite/file", O_RDONLY) == -1 && errno == ENOENT);
    CHECK(my_open("/suite", O_RDONLY) == -1 && errno == EISDIR);
    CHECK(my_unlink("/suite") == -1 && errno == EISDIR);
    CHECK(my_rmdir("/suite") == -1 && errno == ENOTEMPTY);
    CHECK(my_unlink("/suite/.") == -1);
    CHECK(my_rmdir("/") == -1);
    char longname[FILENAME_MAX_LEN + 2];
    memset(longname, 'x', sizeof(longname));
    longname[0] = '/'; longname[sizeof(longname)-1] = '\0';
    CHECK(my_open(longname, O_CREAT | O_RDWR) == -1 && errno == ENAMETOOLONG);
    uint32_t offset = 0;
    my_dirent_t entry;
    int count = 0, r;
    while ((r = my_readdir("/suite", &offset, &entry)) > 0) count++;
    CHECK(r == 0 && count == 4);
    CHECK(my_rmdir("/suite/sub/") == 0);
}

static void descriptor_tests(void) {
    int fd = my_open("/suite/file", O_RDONLY);
    CHECK(fd > 0);
    CHECK(my_write(fd, "x", 1) == -1 && errno == EBADF);
    CHECK(my_read(fd, NULL, 0) == 0);
    CHECK(my_close(fd) == 0);
    CHECK(my_close(fd) == -1 && errno == EBADF);
    fd = my_open("/suite/file", O_WRONLY);
    char c;
    CHECK(fd > 0 && my_read(fd, &c, 1) == -1 && errno == EBADF);
    CHECK(my_close(fd) == 0);
    CHECK(my_open("/suite/file", O_RDONLY | O_TRUNC) == -1);
    int fds[MAXFD - 1];
    for (int i = 0; i < MAXFD - 1; i++) {
        fds[i] = my_open("/suite/file", O_RDONLY);
        CHECK(fds[i] > 0 && fds[i] < MAXFD);
    }
    CHECK(my_open("/suite/file", O_RDONLY) == -1 && errno == EMFILE);
    for (int i = 0; i < MAXFD - 1; i++) CHECK(my_close(fds[i]) == 0);
    CHECK(my_read(MAXFD, &c, 1) == -1);
    CHECK(my_write(-1, &c, 1) == -1);
}

static void lifetime_tests(void) {
    my_fsinfo_t before, after;
    CHECK(my_statfs(&before) == 0);
    put_file("/suite/old", "old", 3);
    int old = my_open("/suite/old", O_RDWR);
    CHECK(old > 0);
    my_stat_t old_stat, new_stat;
    CHECK(my_stat("/suite/old", &old_stat) == 0);
    CHECK(my_unlink("/suite/old") == 0);
    CHECK(my_open("/suite/old", O_RDONLY) == -1 && errno == ENOENT);
    put_file("/suite/new", "new", 3);
    CHECK(my_stat("/suite/new", &new_stat) == 0);
    CHECK(new_stat.inum != old_stat.inum);
    char buf[3];
    CHECK(my_read(old, buf, sizeof(buf)) == 3 && !memcmp(buf, "old", 3));
    CHECK(my_write(old, "!", 1) == 1);
    CHECK(my_close(old) == 0);
    expect_file("/suite/new", "new", 3);
    CHECK(my_unlink("/suite/new") == 0);
    CHECK(my_statfs(&after) == 0);
    CHECK(before.free_inodes == after.free_inodes && before.free_blocks == after.free_blocks);
}

static void cache_tests(void) {
    CHECK(my_mkdir("/many") == 0);
    for (int i = 0; i < 180; i++) {
        char path[32], data[32];
        snprintf(path, sizeof(path), "/many/f%d", i);
        int n = snprintf(data, sizeof(data), "value-%d", i);
        put_file(path, data, (uint32_t)n);
    }
    CHECK(my_sync() == 0);
    for (int i = 0; i < 180; i++) {
        char path[32], data[32];
        snprintf(path, sizeof(path), "/many/f%d", i);
        int n = snprintf(data, sizeof(data), "value-%d", i);
        expect_file(path, data, (uint32_t)n);
        CHECK(my_unlink(path) == 0);
    }
    CHECK(my_rmdir("/many") == 0);
}

static void capacity_tests(void) {
    my_fsinfo_t before, after;
    CHECK(my_statfs(&before) == 0);
    unsigned char *data = malloc(before.max_file_size + 1);
    CHECK(data != NULL);
    for (uint32_t i = 0; i < before.max_file_size + 1; i++) data[i] = (unsigned char)(i % 251);
    int fd = my_open("/suite/large", O_CREAT | O_RDWR);
    CHECK(fd > 0);
    CHECK(my_write(fd, data, before.max_file_size + 1) == (int)before.max_file_size);
    CHECK(my_write(fd, "x", 1) == -1 && errno == EFBIG);
    CHECK(my_close(fd) == 0 && my_sync() == 0);
    expect_file("/suite/large", data, before.max_file_size);
    fd = my_open("/suite/large", O_WRONLY | O_TRUNC);
    CHECK(fd > 0 && my_close(fd) == 0);
    CHECK(my_unlink("/suite/large") == 0);
    CHECK(my_statfs(&after) == 0);
    CHECK(before.free_blocks == after.free_blocks && before.free_inodes == after.free_inodes);
    free(data);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    CHECK(my_sync() == -1 && errno == ENODEV);
    CHECK(my_mount(argv[1], "/bad") == -1);
    CHECK(my_mount(argv[1], "/") == 0);
    CHECK(my_mount(argv[1], "/") == -1 && errno == EBUSY);
    namespace_tests(); descriptor_tests(); lifetime_tests(); cache_tests(); capacity_tests();
    put_file("/suite/file", "abc", 3);
    CHECK(my_sync() == 0);
    int fd = my_open("/suite/file", O_RDWR | O_APPEND);
    char buf[3];
    CHECK(fd > 0 && my_read(fd, buf, 3) == 3 && !memcmp(buf, "abc", 3));
    CHECK(my_write(fd, "def", 3) == 3);
    CHECK(my_close(fd) == 0 && my_sync() == 0);
    CHECK(my_umount("/") == 0 && my_mount(argv[1], "/") == 0);
    expect_file("/suite/file", "abcdef", 6);
    CHECK(my_unlink("/suite/file") == 0 && my_rmdir("/suite") == 0);
    unsigned char binary[BSIZE];
    for (unsigned i = 0; i < sizeof(binary); i++) binary[i] = (unsigned char)(i % 251);
    put_file("/binary-output", binary, sizeof(binary));
    CHECK(my_umount("/") == 0);
    for (int i = 0; i < 8; i++) {
        CHECK(my_mount(argv[1], "/") == 0);
        CHECK(my_umount("/") == 0);
    }
    puts("PASS API: paths, descriptors, unlink lifetime, cache reuse, capacity, remount");
    return 0;
}
