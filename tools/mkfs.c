#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "defs.h"
#include "fs/fs.h"
#include "fs/dir.h"
#include "fs/inode.h"

#define ZERO_BLOCK(buf) memset(buf, 0, BSIZE)

/* Write a single block to disk */
static void write_block(FILE *disk, uint32_t blkno, const void *buf) {
    fseek(disk, blkno * BSIZE, SEEK_SET);
    fwrite(buf, 1, BSIZE, disk);
}

/* Write inode to inode table (each block may store multiple inodes) */
static void write_inode(FILE *disk, uint32_t inode_table_start, uint32_t inum, inode_t *ino) {
    uint32_t block_idx = inum / INODES_PER_BLOCK;
    uint32_t offset_in_block = inum % INODES_PER_BLOCK;
    uint8_t buf[BSIZE];
    ZERO_BLOCK(buf);

    // Read existing block
    fseek(disk, (inode_table_start + block_idx) * BSIZE, SEEK_SET);
    fread(buf, 1, BSIZE, disk);
    // Copy inode to block buffer
    memcpy(buf + offset_in_block * sizeof(inode_t), ino, sizeof(inode_t));

    // Write back
    write_block(disk, inode_table_start + block_idx, buf);
}

/* Add a dirent to a directory's first data block */
static void add_dir_entry(FILE *disk, uint32_t data_start, uint32_t offset, uint32_t inum, const char *name) {
    uint32_t block_no = data_start + (offset / BSIZE) + 1;
    uint32_t off_in_blk = offset % BSIZE;

    uint8_t buf[BSIZE];
    // 先读原来的块
    fseek(disk, block_no * BSIZE, SEEK_SET);
    fread(buf, 1, BSIZE, disk);

    dirent_t de;
    de.inum = inum;
    strncpy(de.name, name, FILENAME_MAX_LEN - 1);

    // 写入空闲位置
    memcpy(buf + (off_in_blk)*sizeof(dirent_t), &de, sizeof(dirent_t));

    // 写回整块
    fseek(disk, block_no * BSIZE, SEEK_SET);
    fwrite(buf, 1, BSIZE, disk);

}

/* ---------------- Main mkfs Function ---------------- */
int main(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "Usage: %s <disk.img> [total_blocks]\n", argv[0]);
        return 1;
    }

    const char *disk_path = argv[1];
    // const char *disk_path = "../disk.img";
    uint32_t total_blocks = (argc >= 3) ? atoi(argv[2]) : BCOUNTS;

    FILE *disk = fopen(disk_path, "wb+");
    if (!disk) { 
        perror("Cannot create disk"); 
        return 1; 
    }

    uint8_t buf[BSIZE];
    ZERO_BLOCK(buf);

    /* ---------------- Disk Layout ---------------- */
    uint32_t blk = 0;
    uint32_t boot_start = blk; blk += BOOT_BLOCKS;
    uint32_t super_start = blk; blk += SUPER_BLOCKS;
    uint32_t log_start = blk; blk += LOG_BLOCKS;
    uint32_t inode_bitmap_start = blk; blk += INODE_BITMAP_BLOCKS;
    uint32_t inode_table_start = blk; blk += INODE_TABLE_BLOCKS;
    uint32_t data_bitmap_start = blk; blk += DATA_BITMAP_BLOCKS;
    uint32_t data_start = blk;
    uint32_t data_blocks = total_blocks - blk;

    printf("[*] Disk layout:\n");
    printf(" Boot Block:     %u\n", boot_start);
    printf(" Super Block:    %u\n", super_start);
    printf(" Log Blocks:     %u - %u\n", log_start, log_start + LOG_BLOCKS - 1);
    printf(" Inode Bitmap:   %u - %u\n", inode_bitmap_start, inode_bitmap_start + INODE_BITMAP_BLOCKS - 1);
    printf(" Inode Table:    %u - %u\n", inode_table_start, inode_table_start + INODE_TABLE_BLOCKS - 1);
    printf(" Data Bitmap:    %u - %u\n", data_bitmap_start, data_bitmap_start + DATA_BITMAP_BLOCKS - 1);
    printf(" Data Blocks:    %u - %u\n", data_start, total_blocks - 1);

    /* ---------------- 1. Boot Block ---------------- */
    ZERO_BLOCK(buf);
    write_block(disk, boot_start, buf);

    /* ---------------- 2. Superblock ---------------- */
    sb_t sb;
    ZERO_BLOCK(&sb);
    sb.magic = FS_MAGIC;
    sb.total_blocks = total_blocks;
    sb.inode_start = inode_table_start;
    sb.inode_blocks = INODE_TABLE_BLOCKS;
    sb.data_start = data_start;
    sb.data_blocks = data_blocks;
    sb.inode_map_start = inode_bitmap_start;
    sb.inode_map_blocks = INODE_BITMAP_BLOCKS;
    sb.data_map_start = data_bitmap_start;
    sb.data_map_blocks = DATA_BITMAP_BLOCKS;
    sb.log_start = log_start;
    sb.log_blocks = LOG_BLOCKS;
    sb.root_inode = ROOT_INODE;

    ZERO_BLOCK(buf);
    memcpy(buf, &sb, sizeof(sb));
    write_block(disk, super_start, buf);

    /* ---------------- 3. Initialize Bitmaps ---------------- */
    ZERO_BLOCK(buf);
    // Inode bitmap: mark ROOT_INODE as used
    BIT_SET(buf, 0);
    BIT_SET(buf, 1);
    BIT_SET(buf, 2);
    write_block(disk, inode_bitmap_start, buf);

    ZERO_BLOCK(buf);
    // Data bitmap: reserve first block for root dir
    for (int i = 0; i < 3; i++) {
        BIT_SET(buf, i);
    }
    write_block(disk, data_bitmap_start, buf);

    /* ---------------- 4. Initialize Root Inode ---------------- */
    inode_t root;
    ZERO_BLOCK(&root);
    root.type = TYPE_DIR;
    root.size = 2 * sizeof(dirent_t); // . and ..
    root.links = 2;
    root.direct[0] = 1; // first data block
    write_inode(disk, inode_table_start, ROOT_INODE, &root);

    /* ---------------- 5. Initialize Root Dir Data Block ---------------- */
    ZERO_BLOCK(buf);
    dirent_t *dir = (dirent_t*)buf;
    dir[0].inum = ROOT_INODE; 
    strcpy(dir[0].name, ".");
    dir[1].inum = ROOT_INODE; 
    strcpy(dir[1].name, "..");
    write_block(disk, data_start + 1, buf);


    const char *execs[] = { "ls","rm","cat","cp","cd", \
                            "mkdir","touch","echo","fdisk", \
                            "usertest","stressfs", "atomtest"};
    uint32_t next_inode = ROOT_INODE + 1; // next free inode
    uint32_t next_dir_offset = root.size; // append after "." and ".."

    for (int i = 0; i < 12; i++) {
        // Create inode
        inode_t ino; 
        ino.type = TYPE_FILE;
        ino.size = 0;
        ino.links = 1;
        ino.direct[0] = 0;
        write_inode(disk, inode_table_start, 0, &ino);

        // Add dirent
        add_dir_entry(disk, data_start, (next_dir_offset)/sizeof(dirent_t), ROOT_INODE, execs[i]);
        next_dir_offset += sizeof(dirent_t);

        // Update root size
        root.size += sizeof(dirent_t);
        next_inode++;
    }

    // readme.txt
    const char *readme_content = "Welcome to your custom filesystem!\n";
    inode_t readme_ino; ZERO_BLOCK(&readme_ino);
    readme_ino.type = TYPE_FILE;
    readme_ino.size = strlen(readme_content);
    readme_ino.links = 1;
    readme_ino.direct[0] = 2; // allocate first free data block for content

    // Write content to data block
    ZERO_BLOCK(buf);
    strncpy((char*)buf, readme_content, BSIZE);
    write_block(disk, data_start + 2, buf);

    write_inode(disk, inode_table_start, 2, &readme_ino);
    add_dir_entry(disk, data_start, (next_dir_offset)/sizeof(dirent_t), ROOT_INODE+1, "README");
    root.size += sizeof(dirent_t);

    // ---------------- 7. Write back updated root inode ----------------
    write_inode(disk, inode_table_start, ROOT_INODE, &root);

    printf("[+] Filesystem created successfully.\n");

    fclose(disk);
    return 0;
}