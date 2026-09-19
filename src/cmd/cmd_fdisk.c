#include "cmd.h"
#include "user.h"
#include <stdio.h>

int cmd_fdisk(int argc, char **argv) {
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "Usage: fdisk\n");
        return -1;
    }
    my_fsinfo_t info;
    if (my_statfs(&info) < 0) {
        perror("fdisk");
        return -1;
    }
    printf("Block size       : %u bytes\n", (unsigned)info.block_size);
    printf("Total blocks     : %u\n", (unsigned)info.total_blocks);
    printf("Data blocks      : %u\n", (unsigned)info.data_blocks);
    printf("Free data blocks : %u\n", (unsigned)info.free_blocks);
    printf("Total inodes     : %u\n", (unsigned)info.total_inodes);
    printf("Free inodes      : %u\n", (unsigned)info.free_inodes);
    printf("Max file size    : %u bytes\n", (unsigned)info.max_file_size);
    return 0;
}
