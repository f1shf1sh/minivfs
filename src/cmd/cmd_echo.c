#include "user.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int cmd_echo(int argc, char **argv) {
    if (argc < 2) {
        printf("\n"); 
        return 0;
    }

    int redirect = 0;     
    char *outfile = NULL;   
    int i;

    // 简单解析重定向
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redirect = 1;
            outfile = argv[i + 1];
            break;
        }
    }

    // 构造输出内容
    char buf[1024] = {0};
    int pos = 0;
    for (i = 1; i < argc; i++) {
        if (redirect && i >= argc - 2) break; // skip '> filename'
        int n = snprintf(buf + pos, sizeof(buf) - pos, "%s%s", argv[i], (i < argc - 1) ? " " : "");
        pos += n;
    }

    if (pos < sizeof(buf) - 1) 
        buf[pos++] = '\n'; // add newline

    if (redirect) {
        int fd = my_open(outfile, O_CREAT | O_RDWR);
        if (fd < 0) {
            printf("echo: cannot open %s\n", outfile);
            return -1;
        }
        my_write(fd, buf, pos);
        my_close(fd);
    } else {
        printf("%s", buf);
    }

    return 0;
}