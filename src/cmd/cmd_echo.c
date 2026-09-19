#include "cmd.h"
#include "user.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_echo(int argc, char **argv) {
    int end = argc;
    const char *outfile = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            if (i + 2 != argc) {
                fprintf(stderr, "Usage: echo [words ...] [> file]\n");
                return -1;
            }
            end = i;
            outfile = argv[i + 1];
            break;
        }
    }

    size_t size = 1;
    for (int i = 1; i < end; i++)
        size += strlen(argv[i]) + (i > 1 ? 1u : 0u);
    char *buf = malloc(size);
    if (!buf)
        return -1;
    size_t pos = 0;
    for (int i = 1; i < end; i++) {
        if (i > 1)
            buf[pos++] = ' ';
        size_t length = strlen(argv[i]);
        memcpy(buf + pos, argv[i], length);
        pos += length;
    }
    buf[pos++] = '\n';

    int result = 0;
    if (outfile) {
        int fd = my_open(outfile, O_CREAT | O_WRONLY | O_TRUNC);
        if (fd < 0) {
            perror("echo: open");
            result = -1;
        } else {
            if (my_write(fd, buf, (uint32_t)pos) != (int)pos) {
                fprintf(stderr, "echo: write failed or was incomplete\n");
                result = -1;
            }
            if (my_close(fd) < 0)
                result = -1;
        }
    } else if (fwrite(buf, 1, pos, stdout) != pos) {
        perror("echo: output");
        result = -1;
    }
    free(buf);
    return result;
}
