#include "cmd.h"
#include "user.h"
#include <fcntl.h>
#include <stdio.h>

int cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: touch <file> [file ...]\n");
        return -1;
    }
    int result = 0;
    for (int i = 1; i < argc; i++) {
        int fd = my_open(argv[i], O_CREAT | O_WRONLY);
        if (fd < 0) {
            perror(argv[i]);
            result = -1;
        } else if (my_close(fd) < 0) {
            perror("touch: close");
            result = -1;
        }
    }
    return result;
}
