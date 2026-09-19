#include "cmd.h"
#include "user.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_DIR "/stressfs.tmp"
#define THREADS 4

typedef struct {
    unsigned id;
    struct timespec deadline;
    unsigned iterations;
    int result;
} worker_arg_t;

static int before_deadline(const struct timespec *deadline) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return -1;
    return now.tv_sec < deadline->tv_sec ||
           (now.tv_sec == deadline->tv_sec && now.tv_nsec < deadline->tv_nsec);
}

static void *file_worker(void *opaque) {
    worker_arg_t *arg = opaque;
    char path[64];
    snprintf(path, sizeof(path), TEST_DIR "/file_%u", arg->id);
    unsigned char expected[BSIZE], actual[BSIZE];
    int fd = -1;
    arg->result = -1;

    do {
        for (unsigned i = 0; i < sizeof(expected); i++)
            expected[i] = (unsigned char)(arg->id * 37u + arg->iterations + i);
        fd = my_open(path, O_CREAT | O_RDWR | O_TRUNC);
        if (fd < 0)
            goto cleanup;
        if (my_write(fd, expected, sizeof(expected)) != (int)sizeof(expected))
            goto cleanup;
        if (my_close(fd) < 0) {
            fd = -1;
            goto cleanup;
        }
        fd = my_open(path, O_RDONLY);
        if (fd < 0)
            goto cleanup;
        if (my_read(fd, actual, sizeof(actual)) != (int)sizeof(actual) ||
            memcmp(expected, actual, sizeof(actual)) != 0 ||
            my_read(fd, actual, 1) != 0)
            goto cleanup;
        if (my_close(fd) < 0) {
            fd = -1;
            goto cleanup;
        }
        fd = -1;
        if (my_unlink(path) < 0)
            goto cleanup;
        arg->iterations++;
        int remaining = before_deadline(&arg->deadline);
        if (remaining < 0)
            goto cleanup;
        if (!remaining)
            break;
    } while (1);
    arg->result = 0;

cleanup:
    if (fd >= 0 && my_close(fd) < 0)
        arg->result = -1;
    if (my_unlink(path) < 0 && errno != ENOENT)
        arg->result = -1;
    return NULL;
}

int cmd_stressfs(int argc, char **argv) {
    long seconds = 1;
    if (argc > 2) {
        fprintf(stderr, "Usage: stressfs [seconds: 1..60]\n");
        return -1;
    }
    if (argc == 2) {
        char *end;
        errno = 0;
        seconds = strtol(argv[1], &end, 10);
        if (errno || end == argv[1] || *end || seconds < 1 || seconds > 60) {
            fprintf(stderr, "stressfs: duration must be 1..60 seconds\n");
            return -1;
        }
    }
    struct timespec deadline;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) < 0)
        return -1;
    deadline.tv_sec += seconds;
    if (my_mkdir(TEST_DIR) < 0) {
        perror("stressfs: cannot create " TEST_DIR);
        return -1;
    }

    pthread_t threads[THREADS];
    worker_arg_t args[THREADS];
    unsigned started = 0;
    for (unsigned i = 0; i < THREADS; i++) {
        args[i] = (worker_arg_t){.id = i, .deadline = deadline, .result = -1};
        int error = pthread_create(&threads[i], NULL, file_worker, &args[i]);
        if (error) {
            fprintf(stderr, "stressfs: pthread_create: %s\n", strerror(error));
            break;
        }
        started++;
    }
    int result = started == THREADS ? 0 : -1;
    unsigned iterations = 0;
    for (unsigned i = 0; i < started; i++) {
        if (pthread_join(threads[i], NULL) != 0 || args[i].result < 0)
            result = -1;
        iterations += args[i].iterations;
    }
    if (my_rmdir(TEST_DIR) < 0)
        result = -1;
    printf("[StressFS] %s: %u threads, %u verified create/write/read/unlink cycles\n",
           result == 0 ? "PASS" : "FAIL", THREADS, iterations);
    return result;
}
