#include "cmd.h"
#include "user.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DIR "/usertest.tmp"
#define TEST_FILE TEST_DIR "/data"

static void fill_pattern(unsigned char *buf, uint32_t offset, uint32_t size) {
    for (uint32_t i = 0; i < size; i++)
        buf[i] = (unsigned char)((offset + i) * 31u + 17u);
}

int cmd_usertest(int argc, char **argv) {
    my_fsinfo_t info;
    if (my_statfs(&info) < 0)
        return -1;
    uint32_t size = info.max_file_size < 1024u * 1024u ?
                    info.max_file_size : 1024u * 1024u;
    if (argc > 2) {
        fprintf(stderr, "Usage: usertest [size_in_MiB]\n");
        return -1;
    }
    if (argc == 2) {
        char *end;
        errno = 0;
        unsigned long mib = strtoul(argv[1], &end, 10);
        if (errno || end == argv[1] || *end || argv[1][0] == '-' ||
            mib == 0 || mib > info.max_file_size / (1024u * 1024u)) {
            fprintf(stderr, "usertest: size must be positive and at most %u bytes\n",
                    (unsigned)info.max_file_size);
            return -1;
        }
        size = (uint32_t)mib * 1024u * 1024u;
    }
    if (my_mkdir(TEST_DIR) < 0) {
        perror("usertest: cannot create " TEST_DIR);
        return -1;
    }

    int result = -1;
    int fd = my_open(TEST_FILE, O_CREAT | O_RDWR | O_TRUNC);
    unsigned char expected[BSIZE], actual[BSIZE];
    if (fd < 0)
        goto cleanup;
    for (uint32_t offset = 0; offset < size;) {
        uint32_t count = size - offset < BSIZE ? size - offset : BSIZE;
        fill_pattern(expected, offset, count);
        if (my_write(fd, expected, count) != (int)count)
            goto cleanup;
        offset += count;
    }
    if (my_close(fd) < 0) {
        fd = -1;
        goto cleanup;
    }
    fd = -1;
    if (my_sync() < 0)
        goto cleanup;
    fd = my_open(TEST_FILE, O_RDONLY);
    if (fd < 0)
        goto cleanup;
    for (uint32_t offset = 0; offset < size;) {
        uint32_t count = size - offset < BSIZE ? size - offset : BSIZE;
        fill_pattern(expected, offset, count);
        if (my_read(fd, actual, count) != (int)count ||
            memcmp(expected, actual, count) != 0)
            goto cleanup;
        offset += count;
    }
    if (my_read(fd, actual, 1) != 0)
        goto cleanup;
    result = 0;

cleanup:
    if (fd >= 0 && my_close(fd) < 0)
        result = -1;
    if (my_unlink(TEST_FILE) < 0 && errno != ENOENT)
        result = -1;
    if (my_rmdir(TEST_DIR) < 0)
        result = -1;
    printf("[UserTest] %s: %u bytes, exact length and byte comparison\n",
           result == 0 ? "PASS" : "FAIL", (unsigned)size);
    return result;
}
