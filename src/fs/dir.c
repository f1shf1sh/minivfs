#include "fs/fs.h"
#include "fs/dir.h"
#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* ======= 目录操作 ======= */
/* helper: compare name safely */
static int name_eq(const char *a, const char *b) {
    return strncmp(a, b, FILENAME_MAX_LEN) == 0;
}

/* dir_lookup: 查找目录项，返回 inode number 或 -ENOENT/-EIO */
int dir_lookup(fs_t *fs, icache_t *dir_ic, const char *name) {
    if (!fs || !dir_ic || !name) 
        return -1;

    /* 目录操作用 inode 的读锁 */
    if (pthread_rwlock_rdlock(&dir_ic->lock) != 0) 
        return -1;

    uint32_t dir_size = dir_ic->inode.size;
    uint32_t offset = 0;
    dirent_t *buf = malloc(BSIZE);

    if (!buf) {
        pthread_rwlock_unlock(&dir_ic->lock);
        return -1;
    }

    while (offset < dir_size) {
        int r = bread(fs, dir_ic, buf, BSIZE, offset);
        if (r <= 0) 
            break;
        int entries = r / DIR_SIZE;
        for (int i = 0; i < entries; i++) {
            if (buf[i].inum != 0 && name_eq(buf[i].name, name)) {
                uint32_t found = buf[i].inum;
                free(buf);
                pthread_rwlock_unlock(&dir_ic->lock);
                return (int)found;
            }
        }
        offset += BSIZE;
    }

    free(buf);
    pthread_rwlock_unlock(&dir_ic->lock);
    return -1;
}

/* dir_add_entry: 添加目录项（线程安全） */
int dir_add(fs_t *fs, icache_t *dir_ino, const char *name, uint32_t inum) {
    if (!fs || !dir_ino || !name) 
        return -1;
    if (strlen(name) >= FILENAME_MAX_LEN) 
        return -1;

    /* 目录写操作需独占 */
    if (pthread_rwlock_wrlock(&dir_ino->lock) != 0) 
        return -1;

    /* 1) 检查是否已经存在同名项 */
    // =============================================
    // int existing = dir_lookup(fs, dir_ino, name); 
    // =============================================
    // 注意：dir_lookup 会再次 lock -> 它会尝试 rdlock，而我们目前持有 wrlock。这是递归/嵌套锁问题。
    /* 上面调用会导致死锁（wrlock -> dir_lookup tries rdlock). 
       为避免嵌套锁问题，我们改为手工在当前写锁下扫描目录而不是调用 dir_lookup. */
    /* So we ignore the earlier call and implement inline below. */

    /* 内联扫描查重 & 记下第一个空槽位置 */
    uint32_t dir_size = dir_ino->inode.size;
    uint32_t offset = 0;
    dirent_t *buf = malloc(BSIZE);
    if (!buf) { 
        pthread_rwlock_unlock(&dir_ino->lock); 
        return -1; 
    }

    int found_dup = 0;
    int found_slot = 0;
    uint32_t slot_offset = 0;

    while (offset < dir_size) {
        int r = bread(fs, dir_ino, buf, BSIZE, offset);
        if (r < 0) { 
            free(buf); 
            pthread_rwlock_unlock(&dir_ino->lock); 
            return -1; 
        }

        int entries = r / sizeof(dirent_t);
        for (int i = 0; i < entries; i++) {
            if (buf[i].inum != 0) {
                if (name_eq(buf[i].name, name)) {
                    found_dup = 1;
                    break;
                }
            } 

            if (!found_slot && buf[i].inum == 0) {
                found_slot = 1;
                slot_offset = offset + i * sizeof(dirent_t);
            }
        }
        if (found_dup) 
            break;
        offset += BSIZE;
    }

    if (found_dup) {
        free(buf);
        pthread_rwlock_unlock(&dir_ino->lock);
        return -1;
    }

    /* 如果没找到空槽，则 slot_offset = dir_size (append at end) */
    if (!found_slot && !found_dup) {
        slot_offset = dir_size;
    }

    /* 准备目录项并写回到 slot_offset */
    dirent_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.inum = inum;
    strncpy(entry.name, name, FILENAME_MAX_LEN - 1);
    /* write into slot (可能是在已有块内也可能是新块) */
    if (bwrite(fs, dir_ino, &entry, sizeof(entry), slot_offset) < 0) {
        free(buf);
        pthread_rwlock_unlock(&dir_ino->lock);
        return -1;
    }

    free(buf);
    pthread_rwlock_unlock(&dir_ino->lock);
    return 0;
}

/* dir_remove_entry: 删除目录项 */
int dir_remove(fs_t *fs, icache_t *dir_ino, const char *name) {
    if (!fs || !dir_ino || !name) 
        return -1;

    if (pthread_rwlock_wrlock(&dir_ino->lock) != 0) 
        return -1;

    uint32_t dir_size = dir_ino->inode.size;
    uint32_t offset = 0;
    dirent_t *buf = malloc(BSIZE);
    if (!buf) { 
        pthread_rwlock_unlock(&dir_ino->lock); 
        return -1; 
    }

    while (offset < dir_size) {
        int r = bread(fs, dir_ino, buf, BSIZE, offset);
        if (r <= 0) break;
        int entries = r / sizeof(dirent_t);
        for (int i = 0; i < entries; i++) {
            if (buf[i].inum != 0 && name_eq(buf[i].name, name)) {
                /* clear this entry */
                buf[i].inum = 0;
                memset(buf[i].name, 0, FILENAME_MAX_LEN);
                if (bwrite(fs, dir_ino, buf, BSIZE, offset) < 0) {
                    free(buf);
                    pthread_rwlock_unlock(&dir_ino->lock);
                    return -1;
                }
                free(buf);
                pthread_rwlock_unlock(&dir_ino->lock);
                return 0;
            }
        }
        offset += BSIZE;
    }

    free(buf);
    pthread_rwlock_unlock(&dir_ino->lock);
    return -1;
}

/* dir_list: 打印目录（调试） */
int dir_list(fs_t *fs, icache_t *dir_ino) {
    if (!fs || !dir_ino) 
        return -1;
    if (pthread_rwlock_rdlock(&dir_ino->lock) != 0) 
        return -1;

    uint32_t dir_size = dir_ino->inode.size;
    uint32_t offset = 0;
    dirent_t *buf = malloc(BSIZE);
    if (!buf) { 
        pthread_rwlock_unlock(&dir_ino->lock); 
        return -1; 
    }

    while (offset < dir_size) {
        int r = bread(fs, dir_ino, buf, BSIZE, offset);
        if (r <= 0) break;
        int entries = r / sizeof(dirent_t);
        for (int i = 0; i < entries; i++) {
            if (buf[i].inum != 0) {
                printf("%s ", buf[i].name);
            }
        }
        printf("\n");
        offset += BSIZE;
    }

    free(buf);
    pthread_rwlock_unlock(&dir_ino->lock);
    return 0;
}