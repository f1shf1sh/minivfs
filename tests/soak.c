#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "user.h"
#include "fs/format.h"

#define WORKER_BYTES ((NDIRECT + 3) * BSIZE + 17)
#define PERSIST_COUNT 96
#define PERSIST_BYTES (BSIZE + 17)

typedef struct {
    int id;
    uint64_t iteration, cycles, written, read;
    double deadline;
    unsigned char expected[WORKER_BYTES], actual[WORKER_BYTES];
} worker_t;

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "signal handling requires lock-free atomic int");
static atomic_int interrupted;
static atomic_int failed;

static void stop_signal(int number) {
    atomic_store_explicit(&interrupted, number, memory_order_relaxed);
}

static double monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static int fail(worker_t *worker, const char *operation, const char *path,
                long actual, long expected) {
    int saved_errno = errno;
    if (!atomic_exchange(&failed, 1)) {
        fprintf(stderr, "SOAK FAIL worker=%d iteration=%" PRIu64
                " operation=%s path=%s actual=%ld expected=%ld errno=%d\n",
                worker->id, worker->iteration, operation, path,
                actual, expected, saved_errno);
        fflush(stderr);
    }
    return -1;
}

#define REQUIRE(worker, operation, path, expression, expected) do { \
    long observed = (expression); \
    if (observed != (long)(expected)) \
        return fail((worker), (operation), (path), observed, (long)(expected)); \
} while (0)

/* A byte sequence derived directly from the recorded worker and iteration. */
static void pattern(unsigned char *bytes, uint32_t size, unsigned identity,
                    uint64_t generation) {
    for (uint32_t i = 0; i < size; i++)
        bytes[i] = (unsigned char)((i + identity * 17u + generation % 251 * 31u) % 251);
    for (unsigned i = 0; i < 8 && i < size; i++)
        bytes[i] = (unsigned char)(generation >> (i * 8));
    for (unsigned i = 0; i < 4 && i + 8 < size; i++)
        bytes[i + 8] = (unsigned char)(identity >> (i * 8));
}

static int put(worker_t *worker, const char *path, uint32_t size) {
    int fd = my_open(path, O_CREAT | O_TRUNC | O_WRONLY);
    if (fd < 0) return fail(worker, "open-write", path, fd, 1);
    for (uint32_t offset = 0; offset < size;) {
        uint32_t amount = MIN(size - offset, BSIZE - 3);
        REQUIRE(worker, "write", path,
                my_write(fd, worker->expected + offset, amount), amount);
        worker->written += amount;
        offset += amount;
    }
    REQUIRE(worker, "close-write", path, my_close(fd), 0);
    return 0;
}

static int expect_fd(worker_t *worker, const char *path, int fd, uint32_t size) {
    for (uint32_t offset = 0; offset < size;) {
        uint32_t amount = MIN(size - offset, BSIZE + 5);
        REQUIRE(worker, "read", path,
                my_read(fd, worker->actual + offset, amount), amount);
        worker->read += amount;
        offset += amount;
    }
    if (memcmp(worker->expected, worker->actual, size)) {
        uint32_t offset = 0;
        while (offset < size && worker->expected[offset] == worker->actual[offset]) offset++;
        fprintf(stderr, "SOAK MISMATCH worker=%d iteration=%" PRIu64
                " path=%s offset=%u expected_byte=%u actual_byte=%u\n",
                worker->id, worker->iteration, path, offset,
                worker->expected[offset], worker->actual[offset]);
        return fail(worker, "memcmp", path, 1, 0);
    }
    unsigned char extra;
    REQUIRE(worker, "eof", path, my_read(fd, &extra, 1), 0);
    return 0;
}

static int expect(worker_t *worker, const char *path, uint32_t size) {
    my_stat_t status;
    REQUIRE(worker, "stat", path, my_stat(path, &status), 0);
    REQUIRE(worker, "size", path, status.size, size);
    int fd = my_open(path, O_RDONLY);
    if (fd < 0) return fail(worker, "open-read", path, fd, 1);
    if (expect_fd(worker, path, fd, size) < 0) return -1;
    REQUIRE(worker, "close-read", path, my_close(fd), 0);
    return 0;
}

static int cycle(worker_t *worker) {
    static const uint32_t sizes[] = {
        0, 1, BSIZE - 1, BSIZE, BSIZE + 1,
        NDIRECT * BSIZE - 1, NDIRECT * BSIZE + 1, WORKER_BYTES
    };
    char path[32];
    snprintf(path, sizeof(path), "/soak/w%d", worker->id);
    uint32_t size = sizes[worker->iteration % (sizeof(sizes) / sizeof(sizes[0]))];
    pattern(worker->expected, size, (unsigned)worker->id, worker->iteration);
    if (put(worker, path, size) < 0 || expect(worker, path, size) < 0) return -1;

    if (worker->iteration % 4 == 1) {
        /* Replace a larger file with a short one and verify truncation. */
        pattern(worker->expected, 17, (unsigned)worker->id, worker->iteration + 1);
        if (put(worker, path, 17) < 0 || expect(worker, path, 17) < 0) return -1;
    } else if (worker->iteration % 4 == 2) {
        /* Keep an old inode open while its name is reused by a new inode. */
        my_stat_t old_status, new_status;
        REQUIRE(worker, "stat-old", path, my_stat(path, &old_status), 0);
        int old_fd = my_open(path, O_RDONLY);
        if (old_fd < 0) return fail(worker, "open-old", path, old_fd, 1);
        REQUIRE(worker, "unlink-open", path, my_unlink(path), 0);
        int missing = my_open(path, O_RDONLY);
        if (missing != -1 || errno != ENOENT)
            return fail(worker, "unlinked-name", path, missing, -1);
        pattern(worker->expected, size, (unsigned)worker->id, worker->iteration + 1);
        if (put(worker, path, size) < 0) return -1;
        REQUIRE(worker, "stat-replacement", path, my_stat(path, &new_status), 0);
        if (old_status.inum == new_status.inum)
            return fail(worker, "live-inode-reused", path, new_status.inum, -1);
        pattern(worker->expected, size, (unsigned)worker->id, worker->iteration);
        if (expect_fd(worker, path, old_fd, size) < 0) return -1;
        REQUIRE(worker, "close-unlinked", path, my_close(old_fd), 0);
        pattern(worker->expected, size, (unsigned)worker->id, worker->iteration + 1);
        if (expect(worker, path, size) < 0) return -1;
    }
    /* Leave names intact after any worker reports failure. */
    if (atomic_load(&failed)) return -1;
    REQUIRE(worker, "unlink", path, my_unlink(path), 0);
    worker->cycles++;
    worker->iteration++;
    return 0;
}

static void *run_worker(void *argument) {
    worker_t *worker = argument;
    while (!atomic_load(&interrupted) && !atomic_load(&failed) &&
           monotonic_seconds() < worker->deadline)
        if (cycle(worker) < 0) break;
    return NULL;
}

static int persistent(worker_t *worker, uint64_t generation, int write_files) {
    worker->iteration = generation;
    for (unsigned i = 0; i < PERSIST_COUNT; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/soak/p%02u", i);
        pattern(worker->expected, PERSIST_BYTES, i + 100, generation);
        if (write_files && put(worker, path, PERSIST_BYTES) < 0) return -1;
        if (expect(worker, path, PERSIST_BYTES) < 0) return -1;
    }
    return 0;
}

static int remount(worker_t *worker, const char *image, int *mounted,
                   uint64_t generation) {
    REQUIRE(worker, "sync", image, my_sync(), 0);
    REQUIRE(worker, "umount", image, my_umount("/"), 0);
    *mounted = 0;
    REQUIRE(worker, "mount", image, my_mount(image, "/"), 0);
    *mounted = 1;
    return persistent(worker, generation, 0);
}

static long argument(const char *text, long minimum, long maximum) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < minimum || value > maximum) return -1;
    return value;
}

int main(int argc, char **argv) {
    long seconds, thread_count, epoch_seconds;
    if (argc != 5 || (seconds = argument(argv[2], 1, 172800)) < 0 ||
        (thread_count = argument(argv[3], 1, 8)) < 0 ||
        (epoch_seconds = argument(argv[4], 1, 60)) < 0) {
        fprintf(stderr, "Usage: %s IMAGE SECONDS(1..172800) THREADS(1..8) EPOCH_SECONDS(1..60)\n", argv[0]);
        return 2;
    }
    struct sigaction action = {.sa_handler = stop_signal};
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    worker_t *workers = calloc((size_t)thread_count, sizeof(*workers));
    worker_t *admin = calloc(1, sizeof(*admin));
    if (!workers || !admin) {
        fprintf(stderr, "SOAK FAIL operation=allocate errno=%d\n", errno);
        free(workers); free(admin);
        return 1;
    }
    admin->id = -1;
    for (long i = 0; i < thread_count; i++) workers[i].id = (int)i;
    int mounted = 0;
    uint64_t epoch = 0;
    double start = monotonic_seconds();
    my_fsinfo_t baseline = {0};
    printf("SOAK START seconds=%ld threads=%ld epoch_seconds=%ld image=%s\n",
           seconds, thread_count, epoch_seconds, argv[1]);
    fflush(stdout);
    if (my_mount(argv[1], "/") < 0) {
        fail(admin, "initial-mount", argv[1], -1, 0);
        goto finish;
    }
    mounted = 1;
    if (my_mkdir("/soak") < 0) {
        fail(admin, "create-new-soak-directory", "/soak", -1, 0);
        goto finish;
    }
    if (persistent(admin, 0, 1) < 0 || remount(admin, argv[1], &mounted, 0) < 0)
        goto finish;
    start = monotonic_seconds();
    double deadline = start + seconds;
    while (!atomic_load(&interrupted) && !atomic_load(&failed) && monotonic_seconds() < deadline) {
        pthread_t threads[8];
        long started = 0;
        double epoch_deadline = MIN(monotonic_seconds() + epoch_seconds, deadline);
        for (long i = 0; i < thread_count; i++) {
            workers[i].deadline = epoch_deadline;
            int error = pthread_create(&threads[i], NULL, run_worker, &workers[i]);
            if (error) {
                errno = error;
                fail(&workers[i], "pthread-create", "/soak", error, 0);
                break;
            }
            started++;
        }
        for (long i = 0; i < started; i++) {
            int error = pthread_join(threads[i], NULL);
            if (error) {
                errno = error;
                fail(&workers[i], "pthread-join", "/soak", error, 0);
            }
        }
        if (atomic_load(&failed) || atomic_load(&interrupted)) break;
        epoch++;
        if (persistent(admin, epoch, 1) < 0 ||
            remount(admin, argv[1], &mounted, epoch) < 0) break;
        my_fsinfo_t current;
        if (my_statfs(&current) < 0) {
            fail(admin, "statfs", "/soak", -1, 0);
            break;
        }
        if (epoch == 1) baseline = current;
        else if (current.free_blocks != baseline.free_blocks ||
                 current.free_inodes != baseline.free_inodes) {
            fprintf(stderr, "SOAK RESOURCES free_blocks=%u expected_blocks=%u"
                    " free_inodes=%u expected_inodes=%u\n", current.free_blocks,
                    baseline.free_blocks, current.free_inodes, baseline.free_inodes);
            fail(admin, "resource-leak", "/soak", 1, 0);
            break;
        }
        uint64_t cycles = 0, written = admin->written, read = admin->read;
        for (long i = 0; i < thread_count; i++) {
            cycles += workers[i].cycles;
            written += workers[i].written;
            read += workers[i].read;
        }
        printf("SOAK PROGRESS elapsed=%.3f epoch=%" PRIu64 " cycles=%" PRIu64
               " written_bytes=%" PRIu64 " read_bytes=%" PRIu64
               " free_blocks=%u free_inodes=%u\n", monotonic_seconds() - start,
               epoch, cycles, written, read, current.free_blocks, current.free_inodes);
        fflush(stdout);
    }
    if (!atomic_load(&failed) && !atomic_load(&interrupted) &&
        (epoch == 0 || monotonic_seconds() - start < seconds))
        fail(admin, "incomplete-duration", "/soak", (long)(monotonic_seconds() - start), seconds);
finish:
    if (mounted) {
        if (atomic_load(&failed)) {
            /* Do not close failed descriptors or reclaim unlinked evidence. */
            if (my_sync() < 0) fprintf(stderr, "SOAK cleanup sync failed errno=%d\n", errno);
        } else if (my_umount("/") < 0) {
            fail(admin, "final-umount", argv[1], -1, 0);
        }
    }
    int signal_number = atomic_load(&interrupted);
    int result = atomic_load(&failed) ? 1 : signal_number ? 130 : 0;
    if (result == 0)
        printf("SOAK PASS elapsed=%.3f epochs=%" PRIu64 " threads=%ld\n",
               monotonic_seconds() - start, epoch, thread_count);
    else if (signal_number)
        printf("SOAK INTERRUPTED signal=%d elapsed=%.3f\n", signal_number,
               monotonic_seconds() - start);
    fflush(stdout);
    free(workers); free(admin);
    return result;
}
