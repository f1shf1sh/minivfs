#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "user.h"    // 你自己的文件系统接口，比如 my_open / my_write / my_read 等

// 简单的CRC32实现
static uint32_t crc32_table[256];

static void init_crc32_table(void) {
    uint32_t poly = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (-(crc & 1) & poly);
        crc32_table[i] = crc;
    }
}

static uint32_t crc32_update(uint32_t crc, const void *buf, size_t len) {
    const uint8_t *p = buf;
    crc = ~crc;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// =============================== //
//        核心测试函数              //
// =============================== //
int cmd_usertest(int argc, char **argv) {
    int fd;
    char buf[BSIZE];
    size_t total_size = 4 * 1024 * 1024; // 默认测试 10MB
    size_t written = 0, readn = 0;
    uint32_t write_crc = 0, read_crc = 0;
    double start, end;

    if (argc > 1) {
        total_size = atol(argv[1]) * 1024 * 1024; // 可自定义大小（MB）
    }

    init_crc32_table();
    srand(time(NULL));

    printf("\n=== [UserTest] File System Stress Test ===\n");
    printf("Target: /a.txt, Size: %.2f MB\n", total_size / (1024.0 * 1024.0));

    // 删除旧文件
    // my_unlink("/a.txt");

    // 创建文件 /a.txt
    fd = my_open("/a.txt", O_CREAT | O_RDWR);
    if (fd < 0) {
        printf("[Error] Failed to open /a.txt\n");
        return -1;
    }
    printf("[Info] Opened /a.txt with fd=%d\n", fd);

    // 写入随机数据并计算写入CRC
    start = clock();
    while (written < total_size) {
        for (int i = 0; i < BSIZE; i++) 
            buf[i] = rand() % 256;
        int n = my_write(fd, buf, BSIZE);
        if (n <= 0) {
            printf("[Error] write failed at %zu bytes\n", written);
            break;
        }
        write_crc = crc32_update(write_crc, buf, n);
        written += n;
    }
    end = clock();

    printf("[Write] %.2f MB written in %.2f s (%.2f MB/s)\n",
           written / (1024.0 * 1024.0),
           (end - start) / CLOCKS_PER_SEC,
           (written / (1024.0 * 1024.0)) / ((end - start) / CLOCKS_PER_SEC));
    printf("[CRC] Write CRC32 = 0x%08X\n", write_crc);

    my_close(fd);

    // 3️⃣ 重新打开并读取校验
    fd = my_open("/a.txt", O_RDONLY);
    if (fd < 0) {
        printf("[Error] reopen /a.txt failed\n");
        return -1;
    }

    start = clock();
    while (1) {
        int n = my_read(fd, buf, BSIZE);
        if (n <= 0) 
            break;
        read_crc = crc32_update(read_crc, buf, n);
        readn += n;
    }
    end = clock();

    printf("[Read] %.2f MB read in %.2f s (%.2f MB/s)\n",
           readn / (1024.0 * 1024.0),
           (end - start) / CLOCKS_PER_SEC,
           (readn / (1024.0 * 1024.0)) / ((end - start) / CLOCKS_PER_SEC));
    printf("[CRC] Read  CRC32 = 0x%08X\n", read_crc);

    my_close(fd);

    // 4️⃣ 验证结果
    if (write_crc == read_crc && written == readn) {
        printf("\n[PASS] CRC matched, data verified successfully.\n");
    } else {
        printf("\n[FAIL] Data mismatch!\n");
    }

    printf("=== [UserTest] Done. ===\n");
    return 0;
}