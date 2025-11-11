#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "user.h"
#include "syscall.h"
#include "fs/fs.h"
#include "fs/inode.h"
#include "fs/dir.h"

#define NUM_THREADS 4
#define NUM_FILES 50
#define FILE_SIZE 8192   // 每个文件 8KB
#define TEST_DURATION 10 // 秒，可改成 12*3600 秒

typedef struct {
    int thread_id;
    int stop;
} worker_ctx_t;

static worker_ctx_t ctx[NUM_THREADS];

// 日志宏，打印线程id和操作
#define LOG(fmt, ...) \
    printf("[Thread %ld] " fmt "\n", pthread_self(), ##__VA_ARGS__)

// 每个线程执行的文件操作
void *worker(void *arg) {
    worker_ctx_t *wctx = (worker_ctx_t *)arg;
    char fname[32];
    char buf[FILE_SIZE];
    memset(buf, 'A' + (rand() % 26), FILE_SIZE);

    while (!wctx->stop) {
        int fidx = rand() % NUM_FILES;
        snprintf(fname, sizeof(fname), "/file%02d.txt", fidx);

        int fd = my_open(fname, O_CREAT | O_RDWR);
        if (fd < 0) {
            LOG("Failed to open %s", fname);
            continue;
        }

        ssize_t w = my_write(fd, buf, FILE_SIZE);
        if (w != FILE_SIZE) LOG("Write incomplete %ld", w);

        lseek(fd, 0, SEEK_SET);
        char readbuf[FILE_SIZE];
        ssize_t r = my_read(fd, readbuf, FILE_SIZE);
        if (r != FILE_SIZE) LOG("Read incomplete %ld", r);

        my_close(fd);

        // // 随机删除文件
        // if (rand() % 10 == 0) {
        //     my_unlink(fname);
        //     LOG("Removed %s", fname);
        // }

        usleep(1000); // 1ms
    }
    return NULL;
}

// cmd 接口
int cmd_stressfs(int argc, char **argv) {
    pthread_t threads[NUM_THREADS];
    srand(time(NULL));

    LOG("Starting stressfs test with %d threads, %d files", NUM_THREADS, NUM_FILES);

    for (int i = 0; i < NUM_THREADS; i++) {
        ctx[i].thread_id = i;
        ctx[i].stop = 0;
        pthread_create(&threads[i], NULL, worker, &ctx[i]);
    }

    // 可控测试时间
    int duration = TEST_DURATION;
    if (argc > 1) duration = atoi(argv[1]);
    sleep(duration);

    // 停止线程
    for (int i = 0; i < NUM_THREADS; i++)
        ctx[i].stop = 1;

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    LOG("Stress test finished.");

    // 输出简单统计信息
    printf("==== FS Stats ====\n");
    printf("Files created: %d\n", NUM_FILES);
    printf("Threads: %d\n", NUM_THREADS);

    return 0;
}