#include <stdio.h>
#include <string.h>
#include "cmd.h"
#include "defs.h"
#include "fs/dir.h"
#include "fs/path.h"
#include "user.h"  

int cmd_rm(int argc, char **argv) {
      if (argc < 2) {
        printf("Usage: rm <filename>\n");
        return -1;
    }

    const char *path = argv[1];
    ctx_t *ctx = get_ctx(); // 获取全局文件系统上下文
    fs_t *fs = &ctx->fs;

    icache_t *dir_ic = NULL;
    char name[FILENAME_MAX_LEN];
    memset(name, 0, FILENAME_MAX_LEN);
    if (path_parent(fs, path, &dir_ic, name) < 0) {
        printf("rm: cannot find parent directory of %s\n", path);
        return -1;
    }

    if (dir_lookup(fs, dir_ic, name) < 0) {
        printf("rm: file not found: %s\n", name);
        return -1;
    }

    // 从父目录删除目录项
    dir_remove(fs, dir_ic, name);

    printf("rm: deleted: %s\n", path);
    return 0;
}