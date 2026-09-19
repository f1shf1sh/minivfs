#include "cmd.h"
#include "user.h"
#include <stdio.h>

static void print_entry(const char *name, uint32_t type, uint32_t size) {
    printf("%-24s\t%c\t%u\n", name,
           type == MY_TYPE_DIR ? 'd' : 'f', (unsigned)size);
}

int cmd_ls(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ls [path]\n");
        return -1;
    }
    const char *path = argc == 2 ? argv[1] : ".";
    my_stat_t stat;
    if (my_stat(path, &stat) < 0) {
        perror("ls: stat");
        return -1;
    }
    printf("Name                    \tType\tSize (bytes)\n");
    if (stat.type == MY_TYPE_FILE) {
        print_entry(path, stat.type, stat.size);
        return 0;
    }

    uint32_t offset = 0;
    my_dirent_t entry;
    int result;
    while ((result = my_readdir(path, &offset, &entry)) > 0)
        print_entry(entry.name, entry.type, entry.size);
    if (result < 0)
        perror("ls: readdir");
    return result;
}
