#include <string.h>
#include <errno.h>
#include "fs/path.h"
#include "fs/dir.h"

static int valid_path(const char *path) {
    if (!path || !path[0]) { errno = EINVAL; return 0; }
    if (strnlen(path, MY_PATH_MAX) == MY_PATH_MAX) { errno = ENAMETOOLONG; return 0; }
    return 1;
}

int path_resolve(fs_t *fs, const char *path, int create, icache_t **out) {
    if (!valid_path(path)) return -1;
    icache_t *cur;
    if (iget(fs, path[0] == '/' ? fs->root_inum : fs->cwd_inum, &cur) < 0)
        return -1;
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (cur->inode.type != TYPE_DIR) { errno = ENOTDIR; goto fail; }
        if (len >= FILENAME_MAX_LEN) { errno = ENAMETOOLONG; goto fail; }
        char name[FILENAME_MAX_LEN];
        memcpy(name, start, len); name[len] = '\0';
        int found = dir_lookup(fs, cur, name);
        if (found == -2 && create && !*p) {
            uint32_t inum;
            if (path_create(fs, cur, name, TYPE_FILE, &inum) < 0) goto fail;
            found = (int)inum;
        }
        if (found < 0) goto fail;
        icache_t *next;
        if (iget(fs, (uint32_t)found, &next) < 0) goto fail;
        iput(fs, cur);
        cur = next;
        if (*p == '/' && cur->inode.type != TYPE_DIR) { errno = ENOTDIR; goto fail; }
    }
    *out = cur;
    return 0;
fail:
    iput(fs, cur);
    return -1;
}

int path_parent(fs_t *fs, const char *path, icache_t **parent, char *name) {
    if (!valid_path(path)) return -1;
    char buf[MY_PATH_MAX];
    size_t len = strlen(path);
    memcpy(buf, path, len + 1);
    while (len > 1 && buf[len - 1] == '/') buf[--len] = '\0';
    char *slash = strrchr(buf, '/');
    const char *child = slash ? slash + 1 : buf;
    size_t child_len = strlen(child);
    if (!child_len || child_len >= FILENAME_MAX_LEN ||
        !strcmp(child, ".") || !strcmp(child, "..")) {
        errno = child_len >= FILENAME_MAX_LEN ? ENAMETOOLONG : EINVAL;
        return -1;
    }
    memcpy(name, child, child_len + 1);
    const char *parent_path = ".";
    if (slash) {
        if (slash == buf) parent_path = "/";
        else { *slash = '\0'; parent_path = buf; }
    }
    if (path_resolve(fs, parent_path, 0, parent) < 0) return -1;
    if ((*parent)->inode.type != TYPE_DIR) {
        iput(fs, *parent);
        errno = ENOTDIR;
        return -1;
    }
    return 0;
}

int path_create(fs_t *fs, icache_t *parent, const char *name,
                uint32_t type, uint32_t *inum_out) {
    int found = dir_lookup(fs, parent, name);
    if (found != -2) { if (found >= 0) errno = EEXIST; return -1; }
    if (type != TYPE_FILE && type != TYPE_DIR) { errno = EINVAL; return -1; }
    int inum = ialloc(fs);
    if (inum < 0) return -1;
    icache_t *created;
    if (iget(fs, (uint32_t)inum, &created) < 0) {
        ifree(fs, (uint32_t)inum);
        return -1;
    }
    created->inode.type = type;
    created->inode.links = 1;
    created->dirty = 1;
    if ((type == TYPE_DIR &&
         (dir_add(fs, created, ".", (uint32_t)inum) < 0 ||
          dir_add(fs, created, "..", parent->inum) < 0)) ||
        dir_add(fs, parent, name, (uint32_t)inum) < 0) {
        created->inode.links = 0;
        created->dirty = 1;
        iput(fs, created);
        return -1;
    }
    *inum_out = (uint32_t)inum;
    return iput(fs, created);
}
