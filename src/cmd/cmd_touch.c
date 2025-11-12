// cmd_touch.c
#include <stdio.h>
#include <string.h>
#include "user.h"

int cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: touch <file>\n");
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        const char *path = argv[i];
        int fd = my_open(path, O_CREAT | O_RDWR);
        if (fd < 0) {
            printf("Failed to create or open file: %s\n", path);
            continue;
        }
        // 不写数据，直接关闭即可
        my_close(fd);
    }

    return 0;
}