#include "cmd.h"
#include "user.h"
#include <stdio.h>

int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mkdir <directory> [directory ...]\n");
        return -1;
    }
    int result = 0;
    for (int i = 1; i < argc; i++) {
        if (my_mkdir(argv[i]) < 0) {
            perror(argv[i]);
            result = -1;
        }
    }
    return result;
}

int cmd_rmdir(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rmdir <directory> [directory ...]\n");
        return -1;
    }
    int result = 0;
    for (int i = 1; i < argc; i++) {
        if (my_rmdir(argv[i]) < 0) {
            perror(argv[i]);
            result = -1;
        }
    }
    return result;
}
