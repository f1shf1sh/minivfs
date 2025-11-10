#pragma once

#include "fs/fs.h"
#include "fs/inode.h"
#include "defs.h"

#define ROOT_INUM 1


// 拆分路径为父目录路径和最后文件名
int path_split(const char *path, char *parent, char *name);

// 核心解析路径，返回目标文件 inode cache
int path_resolve(fs_t *fs, const char *path, int create, icache_t **res_ic);

// 解析父目录，返回父 inode
int path_parent(fs_t *fs, const char *path, icache_t **parent_ic, char *child_name);

// 根据父目录 inode 查找子文件 inode
int path_lookup(fs_t *fs, icache_t *parent_ic, const char *name, uint32_t *res_inum);

// 创建子文件
int path_create(fs_t *fs, icache_t *parent_ic, const char *name, uint32_t type, uint32_t *res_inum);