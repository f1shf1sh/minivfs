#include "fs/format.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_block(int fd, uint32_t block, const void *buffer) {
    size_t done = 0;
    off_t offset = (off_t)block * BSIZE;
    while (done < BSIZE) {
        ssize_t count = pwrite(fd, (const char *)buffer + done,
                               BSIZE - done, offset + (off_t)done);
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

static int format_image(int fd, uint32_t total_blocks) {
    if (ftruncate(fd, (off_t)total_blocks * BSIZE) < 0)
        return -1;

    sb_t sb = {
        .magic = FS_MAGIC,
        .total_blocks = total_blocks,
        .block_size = BSIZE,
        .log_start = LOG_START,
        .log_blocks = LOG_BLOCKS,
        .inode_map_start = INODE_MAP_START,
        .inode_map_blocks = INODE_BITMAP_BLOCKS,
        .inode_start = INODE_TABLE_START,
        .inode_blocks = INODE_TABLE_BLOCKS,
        .data_map_start = DATA_MAP_START,
        .data_map_blocks = DATA_BITMAP_BLOCKS,
        .data_start = DATA_START,
        .data_blocks = total_blocks - DATA_START,
        .root_inode = ROOT_INODE
    };
    uint8_t buf[BSIZE] = {0};
    memcpy(buf, &sb, sizeof(sb));
    if (write_block(fd, BOOT_BLOCKS, buf) < 0)
        return -1;

    /* Reserve inode/data slot zero, then root and README. */
    memset(buf, 0, sizeof(buf));
    BIT_SET(buf, 0);
    BIT_SET(buf, ROOT_INODE);
    BIT_SET(buf, 2);
    if (write_block(fd, INODE_MAP_START, buf) < 0 ||
        write_block(fd, DATA_MAP_START, buf) < 0)
        return -1;

    const char content[] = "Welcome to your custom filesystem!\n";
    inode_t root = {
        .type = TYPE_DIR,
        .links = 2,
        .size = 3 * sizeof(dirent_t),
        .direct = {1}
    };
    inode_t readme = {
        .type = TYPE_FILE,
        .links = 1,
        .size = sizeof(content) - 1,
        .direct = {2}
    };
    memset(buf, 0, sizeof(buf));
    memcpy(buf + ROOT_INODE * sizeof(inode_t), &root, sizeof(root));
    memcpy(buf + 2 * sizeof(inode_t), &readme, sizeof(readme));
    if (write_block(fd, INODE_TABLE_START, buf) < 0)
        return -1;

    const dirent_t entries[] = {
        {.inum = ROOT_INODE, .name = "."},
        {.inum = ROOT_INODE, .name = ".."},
        {.inum = 2, .name = "README"}
    };
    memset(buf, 0, sizeof(buf));
    memcpy(buf, entries, sizeof(entries));
    if (write_block(fd, DATA_START + 1, buf) < 0)
        return -1;
    memset(buf, 0, sizeof(buf));
    memcpy(buf, content, sizeof(content) - 1);
    if (write_block(fd, DATA_START + 2, buf) < 0)
        return -1;

    int result;
    do {
        result = fsync(fd);
    } while (result < 0 && errno == EINTR);
    return result;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <disk.img> [total_blocks]\n", argv[0]);
        return 1;
    }

    unsigned long blocks = BCOUNTS;
    if (argc == 3) {
        char *end;
        errno = 0;
        blocks = strtoul(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0' ||
            argv[2][0] == '-' || blocks < MIN_IMAGE_BLOCKS || blocks > MAX_IMAGE_BLOCKS) {
            fprintf(stderr, "total_blocks must be between %u and %u\n",
                    (unsigned)MIN_IMAGE_BLOCKS, (unsigned)MAX_IMAGE_BLOCKS);
            return 1;
        }
    }

    int fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Cannot create image");
        return 1;
    }
    if (format_image(fd, (uint32_t)blocks) < 0) {
        perror("Cannot format image");
        close(fd);
        return 1;
    }
    if (close(fd) < 0) {
        perror("Cannot close image");
        return 1;
    }
    printf("Created %s: %lu blocks of %u bytes\n", argv[1], blocks, (unsigned)BSIZE);
    return 0;
}
