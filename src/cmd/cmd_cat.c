#include <stdio.h>
#include <string.h>
#include "user.h"
#include "fs/dir.h"
#include "fs/path.h"
#include "fs/fs.h"
#include "fs/inode.h"

int cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: cat <file>\n");
        return -1;
    }

    int fd = my_open(argv[1], O_RDONLY);

    if (fd <= 0)
        printf("cat: %s: No such file or directory\n", argv[1]);
    
    char buf[BSIZE];
    while (1) {
        memset(buf, 0, BSIZE);
        int n = my_read(fd, buf, BSIZE);
        if (n <= 0) 
            break;
        printf("%s", buf);
    }
    
    return 0;
}