#include <stdio.h>
#include "cmd.h"
#include "defs.h"
#include "fs/dir.h"
#include "fs/path.h"
#include "user.h"  

int cmd_ls(int argc, char **argv) {
     const char *path = ".";  // default
    if (argc >= 2)
        path = argv[1];
    
    icache_t *ic = NULL;
    ctx_t *ctx = get_ctx();
    int r = path_resolve(&ctx->fs, path, 0, &ic);
    if (r < 0) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
    }

    dir_list(&ctx->fs, ic);

    return 0;
}