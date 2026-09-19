#include "cmd.h"
#include "user.h"
#include <fcntl.h>
#include <stdio.h>

int cmd_cat(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: cat <file>\n");
        return -1;
    }
    int fd = my_open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("cat: open");
        return -1;
    }

    char buf[BSIZE];
    int count;
    int result = 0;
    while ((count = my_read(fd, buf, sizeof(buf))) > 0) {
        if (fwrite(buf, 1, (size_t)count, stdout) != (size_t)count) {
            perror("cat: output");
            result = -1;
            break;
        }
    }
    if (count < 0) {
        perror("cat: read");
        result = -1;
    }
    if (my_close(fd) < 0) {
        perror("cat: close");
        result = -1;
    }
    return result;
}
