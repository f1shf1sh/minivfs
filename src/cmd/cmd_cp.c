#include "user.h"
#include <stdio.h>
#include <stdlib.h>


int cmd_cp(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: cp <source> <dest>\n");
        return -1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    int fd_src = my_open(src, O_RDONLY);
    if (fd_src < 0) {
        printf("cp: cannot open source file %s\n", src);
        return -1;
    }

    int fd_dst = my_open(dst, O_CREAT | O_RDWR);
    if (fd_dst < 0) {
        printf("cp: cannot create destination file %s\n", dst);
        my_close(fd_src);
        return -1;
    }

    char *buf = malloc(BSIZE);
    if (!buf) {
        printf("cp: memory allocation failed\n");
        my_close(fd_src);
        my_close(fd_dst);
        return -1;
    }

    int n;
    while ((n = my_read(fd_src, buf, BSIZE)) > 0) {
        int written = my_write(fd_dst, buf, n);
        if (written != n) {
            printf("cp: write error\n");
            free(buf);
            my_close(fd_src);
            my_close(fd_dst);
            return -1;
        }
    }

    free(buf);
    my_close(fd_src);
    my_close(fd_dst);
    return 0;
}