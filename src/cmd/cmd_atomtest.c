#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "user.h"

#define N_THREADS 1000
#define FILE_NAME "/test_atomic.txt"
#define LOOP 100000

void *writer(void *arg) {
    char *pattern = (char*)arg;
    for (int i = 0; i < LOOP; i++) {
        int fd = my_open(FILE_NAME, O_RDWR | O_CREAT);
        if (fd < 0) continue;
        my_write(fd, pattern, strlen(pattern));
        my_close(fd);
    }
    return NULL;
}

void *reader(void *arg) {
    char buf[1024];
    for (int i = 0; i < LOOP; i++) {
        int fd = my_open(FILE_NAME, O_RDONLY);
        if (fd < 0) continue;
        int n = my_read(fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            // 检查是否是完整模式
            if (strcmp(buf, "AAAA") != 0 && strcmp(buf, "BBBB") != 0) {
                printf("Read inconsistent data: %s\n", buf);
            }
        }
        my_close(fd);
    }
    return NULL;
}

int cmd_atomtest(int argc, char *argv[]) {
    pthread_t writers[2], readers[2];

    pthread_create(&writers[0], NULL, writer, "AAAA");
    pthread_create(&writers[1], NULL, writer, "BBBB");

    pthread_create(&readers[0], NULL, reader, NULL);
    pthread_create(&readers[1], NULL, reader, NULL);

    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
        pthread_join(readers[i], NULL);
    }

    return 0;
}