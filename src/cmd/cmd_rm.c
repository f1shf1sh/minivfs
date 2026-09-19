#include "cmd.h"
#include "user.h"
#include <stdio.h>

int cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rm <file> [file ...]\n");
        return -1;
    }
    int result = 0;
    for (int i = 1; i < argc; i++) {
        if (my_unlink(argv[i]) < 0) {
            perror(argv[i]);
            result = -1;
        }
    }
    return result;
}
