#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fs/fs.h"
#include "fs/dir.h"
#include "fs/path.h"

static void skip_slash(const char **p) {
    while (**p == '/') (*p)++;
}

// 拆分路径为父目录和文件名
int path_split(const char *path, char *parent, char *name) {
    if (!path || !parent || !name) return -1;
    size_t len = strlen(path);
    if (len == 0) 
        return -1;

    const char *slash = strrchr(path, '/');
    if (!slash) {
        strcpy(parent, ".");
        strcpy(name, path);
    } else {
        size_t plen = slash - path;
        strncpy(parent, path, plen);
        parent[plen] = '\0';
        strcpy(name, slash + 1);
        if (strlen(parent) == 0) strcpy(parent, "/");
    }
    return 0;
}

// 核心路径解析
int path_resolve(fs_t *fs, const char *path, int create, icache_t **res_ic) {
    if (!fs || !path || !res_ic) 
        return -1;

    icache_t *cur_ic = NULL;
    uint32_t cur_inum;

    // 绝对路径从 root 开始
    if (path[0] == '/') 
        cur_inum = fs->root_inum;
    else 
        cur_inum = fs->cwd_inum;  // 相对路径从 cwd 开始

    if (iget(fs, cur_inum, &cur_ic) < 0) 
        return -1;

    // 使用栈处理路径段
    // int MAX_PATH_DEPTH = 128;
    char *components[128];
    int depth = 0;

    const char *p = path;
    skip_slash(&p);
    while (*p) {
        char buf[FILENAME_MAX];
        const char *start = p;
        while (*p && *p != '/') 
            p++;
        size_t len = p - start;
        if (len >= FILENAME_MAX) 
            len = FILENAME_MAX - 1;
        strncpy(buf, start, len);
        buf[len] = '\0';
        skip_slash(&p);

        if (strcmp(buf, ".") == 0) 
            continue;
        if (strcmp(buf, "..") == 0) {
            if (depth > 0) 
            depth--;
            continue;
        }
        components[depth++] = strdup(buf);
    }

    // 遍历栈找到最终 inode
    for (int i = 0; i < depth; i++) {
        int next_inum = dir_lookup(fs, cur_ic, components[i]);
        if (next_inum < 0) {
            if (create && i == depth - 1) {
                // 最后一个文件可创建
                if (path_create(fs, cur_ic, components[i], TYPE_FILE, &next_inum) < 0) {
                    iput(fs, &cur_ic);
                    return -1;
                }
            } else {
                iput(fs, &cur_ic);
                return -1;
            }
        }

        icache_t *next_ic = NULL;
        if (iget(fs, next_inum, &next_ic) < 0) {
            iput(fs, &cur_ic);
            return -1;
        }

        iput(fs, &cur_ic);
        cur_ic = next_ic;
    }

    *res_ic = cur_ic;
    return 0;
}

// 获取父目录 inode
int path_parent(fs_t *fs, const char *path, icache_t **parent_ic, char *child_name) {
    char parent[FILENAME_MAX];
    if (path_split(path, parent, child_name) < 0) 
        return -1;
    return path_resolve(fs, parent, 0, parent_ic);
}

// 查找子文件 inode
int path_lookup(fs_t *fs, icache_t *parent_ic, const char *name, uint32_t *res_inum) {
    return dir_lookup(fs, parent_ic, name);
}

// 创建子文件
int path_create(fs_t *fs, icache_t *parent_ic, const char *name, uint32_t type, uint32_t *res_inum) {
    // 分配 inode
    uint32_t inum = ialloc(fs);
    if (inum == 0) 
        return -1;

    icache_t c = {0};
    c.inode.type = type;
    if (iwrite(fs, inum, &c) < 0) 
        return -1;

    if (dir_add(fs, parent_ic, name, inum) < 0) {
        ifree(fs, inum);
        return -1;
    }

    *res_inum = inum;
    return 0;
}