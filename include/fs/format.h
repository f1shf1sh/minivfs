#pragma once

#include <stdint.h>
#include "defs.h"

/* The image format is shared by mkfs and the filesystem runtime. */
#define FS_MAGIC 0x68736966
#define BOOT_BLOCKS 1
#define SUPER_BLOCKS 1
#define LOG_BLOCKS 32
#define INODE_BITMAP_BLOCKS 2
#define INODE_TABLE_BLOCKS 128
#define DATA_BITMAP_BLOCKS 2
#define ROOT_INODE 1

#define LOG_START (BOOT_BLOCKS + SUPER_BLOCKS)
#define INODE_MAP_START (LOG_START + LOG_BLOCKS)
#define INODE_TABLE_START (INODE_MAP_START + INODE_BITMAP_BLOCKS)
#define DATA_MAP_START (INODE_TABLE_START + INODE_TABLE_BLOCKS)
#define DATA_START (DATA_MAP_START + DATA_BITMAP_BLOCKS)
#define MIN_IMAGE_BLOCKS (DATA_START + 3)
#define MAX_IMAGE_BLOCKS (DATA_START + DATA_BITMAP_BLOCKS * BSIZE * 8)

typedef struct superblock {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t block_size;
    uint32_t log_start;
    uint32_t log_blocks;
    uint32_t inode_map_start;
    uint32_t inode_map_blocks;
    uint32_t inode_start;
    uint32_t inode_blocks;
    uint32_t data_map_start;
    uint32_t data_map_blocks;
    uint32_t data_start;
    uint32_t data_blocks;
    uint32_t root_inode;
    uint32_t reserved[8];
} sb_t;

#define NDIRECT 12
#define INODE_NINDIRECT 1

typedef enum {
    TYPE_FREE = 0,
    TYPE_FILE = 1,
    TYPE_DIR = 2
} inode_type_t;

/* Block addresses are relative to data_start; zero means no block. */
typedef struct inode {
    uint16_t type;
    uint16_t links;
    uint32_t size;
    uint32_t direct[NDIRECT + INODE_NINDIRECT];
    uint8_t pad[4];
} inode_t;

typedef struct dirent {
    uint32_t inum;
    char name[FILENAME_MAX_LEN];
} dirent_t;

#define INODE_SIZE 64
#define INODES_PER_BLOCK (BSIZE / sizeof(inode_t))
#define INODE_BLOCK(inum) ((inum) / INODES_PER_BLOCK)
#define INODE_OFFSET(inum) ((inum) % INODES_PER_BLOCK)
#define INDIRECT_ENTRIES (BSIZE / sizeof(uint32_t))
#define MAX_FILE_SIZE ((NDIRECT + INDIRECT_ENTRIES) * BSIZE)
#define DIR_SIZE 64
#define DIR_ENTRIES_PER_BLOCK (BSIZE / sizeof(dirent_t))

_Static_assert(sizeof(sb_t) == 88, "superblock format changed");
_Static_assert(sizeof(inode_t) == INODE_SIZE, "inode format changed");
_Static_assert(sizeof(dirent_t) == DIR_SIZE, "directory format changed");
