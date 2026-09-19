#include <string.h>
#include <errno.h>
#include "fs/dir.h"

int dir_read_entry(fs_t *fs, icache_t *dir, uint32_t *offset, dirent_t *out) {
    if (dir->inode.type != TYPE_DIR) { errno = ENOTDIR; return -1; }
    if (*offset % sizeof(*out)) { errno = EINVAL; return -1; }
    while (*offset < dir->inode.size) {
        int n = bread(fs, dir, out, sizeof(*out), *offset);
        if (n != (int)sizeof(*out)) { if (n >= 0) errno = EIO; return -1; }
        *offset += sizeof(*out);
        if (out->inum) {
            if (!memchr(out->name, '\0', sizeof(out->name))) { errno = EIO; return -1; }
            return 1;
        }
    }
    return 0;
}

int dir_lookup(fs_t *fs, icache_t *dir, const char *name) {
    uint32_t offset = 0;
    dirent_t entry;
    int r;
    while ((r = dir_read_entry(fs, dir, &offset, &entry)) > 0) {
        if (!strcmp(entry.name, name)) return (int)entry.inum;
    }
    /* -2 distinguishes a missing entry from an I/O or format error. */
    if (r < 0) return -1;
    errno = ENOENT;
    return -2;
}

int dir_add(fs_t *fs, icache_t *dir, const char *name, uint32_t inum) {
    size_t len = strlen(name);
    if (!len || len >= FILENAME_MAX_LEN || strchr(name, '/')) {
        errno = len >= FILENAME_MAX_LEN ? ENAMETOOLONG : EINVAL;
        return -1;
    }
    int found = dir_lookup(fs, dir, name);
    if (found != -2) { if (found >= 0) errno = EEXIST; return -1; }
    uint32_t slot = dir->inode.size;
    dirent_t entry;
    for (uint32_t off = 0; off < dir->inode.size; off += sizeof(entry)) {
        if (bread(fs, dir, &entry, sizeof(entry), off) != (int)sizeof(entry))
            return -1;
        if (!entry.inum) { slot = off; break; }
    }
    memset(&entry, 0, sizeof(entry));
    entry.inum = inum;
    memcpy(entry.name, name, len + 1);
    return bwrite(fs, dir, &entry, sizeof(entry), slot, 1) == (int)sizeof(entry)
        ? 0 : -1;
}

int dir_remove(fs_t *fs, icache_t *dir, const char *name) {
    if (!strcmp(name, ".") || !strcmp(name, "..")) { errno = EINVAL; return -1; }
    uint32_t offset = 0;
    dirent_t entry;
    int r;
    while ((r = dir_read_entry(fs, dir, &offset, &entry)) > 0) {
        if (!strcmp(entry.name, name)) {
            memset(&entry, 0, sizeof(entry));
            return bwrite(fs, dir, &entry, sizeof(entry), offset - sizeof(entry), 0)
                == (int)sizeof(entry) ? 0 : -1;
        }
    }
    if (r == 0) errno = ENOENT;
    return -1;
}

int dir_is_empty(fs_t *fs, icache_t *dir) {
    uint32_t offset = 0;
    dirent_t entry;
    int r;
    while ((r = dir_read_entry(fs, dir, &offset, &entry)) > 0) {
        if (strcmp(entry.name, ".") && strcmp(entry.name, "..")) return 0;
    }
    return r < 0 ? -1 : 1;
}
