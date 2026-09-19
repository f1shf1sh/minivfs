#include "cmd.h"
#include "user.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define TEST_DIR "/atomtest.tmp"
#define TEST_FILE TEST_DIR "/append"
#define THREADS 4
#define RECORDS 128
#define RECORD_SIZE 16

typedef struct {
    unsigned id;
    int result;
} writer_arg_t;

static void make_record(unsigned char *record, unsigned id, unsigned sequence) {
    record[0] = (unsigned char)id;
    record[1] = (unsigned char)(sequence >> 8);
    record[2] = (unsigned char)sequence;
    for (unsigned i = 3; i < RECORD_SIZE; i++)
        record[i] = (unsigned char)(id + sequence + i);
}

static void *append_writer(void *opaque) {
    writer_arg_t *arg = opaque;
    arg->result = -1;
    int fd = my_open(TEST_FILE, O_WRONLY | O_APPEND);
    if (fd < 0)
        return NULL;
    unsigned char record[RECORD_SIZE];
    unsigned sequence;
    for (sequence = 0; sequence < RECORDS; sequence++) {
        make_record(record, arg->id, sequence);
        if (my_write(fd, record, sizeof(record)) != (int)sizeof(record))
            break;
    }
    if (my_close(fd) == 0 && sequence == RECORDS)
        arg->result = 0;
    return NULL;
}

int cmd_atomtest(int argc, char **argv) {
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "Usage: atomtest\n");
        return -1;
    }
    my_fsinfo_t info;
    if (my_statfs(&info) < 0 ||
        info.max_file_size < THREADS * RECORDS * RECORD_SIZE)
        return -1;
    if (my_mkdir(TEST_DIR) < 0) {
        perror("atomtest: cannot create " TEST_DIR);
        return -1;
    }

    int result = -1;
    int fd = my_open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0)
        goto cleanup;
    if (my_close(fd) < 0) {
        fd = -1;
        goto cleanup;
    }
    fd = -1;

    pthread_t threads[THREADS];
    writer_arg_t args[THREADS];
    unsigned started = 0;
    for (unsigned i = 0; i < THREADS; i++) {
        args[i].id = i;
        args[i].result = -1;
        int error = pthread_create(&threads[i], NULL, append_writer, &args[i]);
        if (error) {
            fprintf(stderr, "atomtest: pthread_create: %s\n", strerror(error));
            break;
        }
        started++;
    }
    int failed = started != THREADS;
    for (unsigned i = 0; i < started; i++) {
        if (pthread_join(threads[i], NULL) != 0 || args[i].result < 0)
            failed = 1;
    }
    if (failed)
        goto cleanup;

    fd = my_open(TEST_FILE, O_RDONLY);
    if (fd < 0)
        goto cleanup;
    unsigned char seen[THREADS][RECORDS] = {{0}};
    unsigned char actual[RECORD_SIZE], expected[RECORD_SIZE];
    for (unsigned i = 0; i < THREADS * RECORDS; i++) {
        if (my_read(fd, actual, sizeof(actual)) != (int)sizeof(actual))
            goto cleanup;
        unsigned id = actual[0];
        unsigned sequence = ((unsigned)actual[1] << 8) | actual[2];
        if (id >= THREADS || sequence >= RECORDS || seen[id][sequence])
            goto cleanup;
        make_record(expected, id, sequence);
        if (memcmp(expected, actual, sizeof(actual)) != 0)
            goto cleanup;
        seen[id][sequence] = 1;
    }
    if (my_read(fd, actual, 1) != 0)
        goto cleanup;
    result = 0;

cleanup:
    if (fd >= 0 && my_close(fd) < 0)
        result = -1;
    if (my_unlink(TEST_FILE) < 0 && errno != ENOENT)
        result = -1;
    if (my_rmdir(TEST_DIR) < 0)
        result = -1;
    printf("[AtomTest] %s: %u threads, %u complete unique append records\n",
           result == 0 ? "PASS" : "FAIL", THREADS, THREADS * RECORDS);
    return result;
}
