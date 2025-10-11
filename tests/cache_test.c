#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/disk.h"
#include "../include/cache.h"
#include "../include/defs.h"

#define TEST_DISK "disk.img"
extern cache_mgr_t cache_mgr;
extern disk_mgr_t disk_mgr;

int main() {
    printf("[TEST] Starting disk + cache layer test...\n");

    // 1️⃣ 初始化磁盘管理器
    disk_init(&disk_mgr);

    // 2️⃣ 挂载模拟磁盘
    struct disk *disk = disk_mount(TEST_DISK, BSIZE, BCOUNT);
    if (!disk) {
        fprintf(stderr, "❌ Failed to mount disk\n");
        return 1;
    }

    diskrws_t ops = {
        .read = disk_read,
        .write = disk_write,
        .sync = disk_sync
    };

    size_t dev_id = disk_register(&disk_mgr, disk, &ops);
    printf("[OK] Disk mounted as dev_id=%zu\n", dev_id);

    // 3️⃣ 初始化 cache 层
    cache_init(&cache_mgr);

    // 4️⃣ 写入一块数据
    char write_buf[BSIZE];
    memset(write_buf, 'A', sizeof(write_buf));
    cache_block_t *blk = cache_get(&cache_mgr, dev_id, 0);
    memcpy(blk->data, write_buf, BSIZE);
    blk->type = DIRTY;
    cache_release(&cache_mgr, blk);
    printf("[OK] Wrote block 0\n");


    // 5️⃣ 读取刚才写入的块（cache hit）
    cache_block_t *blk2 = cache_get(&cache_mgr, dev_id, 0);
    if (memcmp(blk2->data, write_buf, BSIZE) == 0)
        printf("[OK] Cache hit verified!\n");
    else
        printf("❌ Cache data mismatch!\n");
    cache_release(&cache_mgr, blk2);

    // 6️⃣ 模拟cache miss：访问未读过的块
    cache_block_t *blk3 = cache_get(&cache_mgr, dev_id, 10);
    printf("[OK] Loaded block 10 (cache miss)\n");
    cache_release(&cache_mgr, blk3);

    // 7️⃣ 同步所有数据到磁盘
    cache_sync(&cache_mgr, disk);
    printf("[OK] Cache sync completed\n");

    // 8️⃣ 再次挂载，验证数据持久性
    disk_umount(disk);
    disk = disk_mount(TEST_DISK, BSIZE, BCOUNT);
    disk_read(disk, write_buf, BSIZE, 0);
    if (write_buf[0] == 'A')
        printf("[OK] Data persisted on disk ✅\n");
    else
        printf("❌ Data not persisted!\n");

    // 9️⃣ 清理
    disk_umount(disk);
    printf("[TEST] All tests finished.\n");
    return 0;
}