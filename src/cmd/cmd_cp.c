#include "cmd.h"
#include "user.h"
#include <fcntl.h>
#include <stdio.h>

int cmd_cp(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: cp <source> <dest>\n");
        return -1;
    }
    my_stat_t source, target;
    if (my_stat(argv[1], &source) < 0) {
        perror("cp: source");
        return -1;
    }
    if (source.type != MY_TYPE_FILE) {
        fprintf(stderr, "cp: source must be a regular file\n");
        return -1;
    }
    if (my_stat(argv[2], &target) == 0 && source.inum == target.inum) {
        fprintf(stderr, "cp: source and destination are the same file\n");
        return -1;
    }
    int fd_source = my_open(argv[1], O_RDONLY);
    if (fd_source < 0) {
        perror("cp: open source");
        return -1;
    }
    int fd_target = my_open(argv[2], O_CREAT | O_WRONLY | O_TRUNC);
    if (fd_target < 0) {
        perror("cp: open destination");
        my_close(fd_source);
        return -1;
    }

    char buf[BSIZE];
    int count;
    int result = 0;
    while ((count = my_read(fd_source, buf, sizeof(buf))) > 0) {
        if (my_write(fd_target, buf, (uint32_t)count) != count) {
            fprintf(stderr, "cp: write failed or was incomplete\n");
            result = -1;
            break;
        }
    }
    if (count < 0) {
        perror("cp: read");
        result = -1;
    }
    if (my_close(fd_source) < 0)
        result = -1;
    if (my_close(fd_target) < 0)
        result = -1;
    return result;
}
