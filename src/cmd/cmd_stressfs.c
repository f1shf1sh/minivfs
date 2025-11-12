// cmd_stressfs.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/statvfs.h>
#include "user.h"

#define NUM_FILES 5
#define NUM_THREADS 4
#define MAX_FILENAME 60
#define BUF_SIZE 1024

typedef struct task {
    char cmd[16];
    char path[MAX_FILENAME];
    char target[MAX_FILENAME]; // cp目标
    char data[BUF_SIZE];       // write数据
    struct task *next;
} task_t;

typedef struct {
    task_t *head;
    task_t *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} task_queue_t;

static task_queue_t queue = {0};
static int stop_flag = 0;
static pthread_mutex_t file_locks[NUM_FILES];

// ---------- Queue Operations ----------
void enqueue(task_queue_t *q, task_t *t) {
    pthread_mutex_lock(&q->lock);
    t->next = NULL;
    if (!q->tail) {
        q->head = q->tail = t;
    } else {
        q->tail->next = t;
        q->tail = t;
    }
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

task_t* dequeue(task_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    while (!q->head && !stop_flag)
        pthread_cond_wait(&q->cond, &q->lock);
    task_t *t = q->head;
    if (t) {
        q->head = t->next;
        if (!q->head) q->tail = NULL;
    }
    pthread_mutex_unlock(&q->lock);
    return t;
}

// ---------- Helper ----------
int get_file_index(const char *path) {
    // /file_12.txt -> 12
    int idx = 0;
    sscanf(path, "/file_%d.txt", &idx);
    if (idx < 0) idx = 0;
    if (idx >= NUM_FILES) idx = NUM_FILES -1;
    return idx;
}

// ---------- Worker Thread ----------
void* worker(void *arg) {
    (void)arg;
    char buf[BUF_SIZE];

    while (1) {
        task_t *task = dequeue(&queue);
        if (!task) break;

        int idx = get_file_index(task->path);
        pthread_mutex_lock(&file_locks[idx]);

        if (strcmp(task->cmd, "ls") == 0) {
            cmd_ls(1, (char*[]){task->path});
        } else if (strcmp(task->cmd, "cat") == 0) {
            cmd_cat(2, (char*[]){task->cmd, task->path});
        } else if (strcmp(task->cmd, "rm") == 0) {
            cmd_rm(2, (char*[]){task->cmd, task->path});
        } else if (strcmp(task->cmd, "cp") == 0) {
            cmd_cp(3, (char*[]){task->cmd, task->path, task->target});
        } else if (strcmp(task->cmd, "write") == 0) {
            int fd = my_open(task->path, O_CREAT|O_RDWR);
            if (fd >= 0) {
                my_write(fd, task->data, strlen(task->data));
                my_close(fd);
            }
        }

        pthread_mutex_unlock(&file_locks[idx]);
        free(task);
    }
    return NULL;
}

// ---------- Task Generator ----------
void generate_tasks(int num_files) {
    char filename[64];
    char target[64];
    const char *cmds[] = { "ls", "write", "cat", "rm", "cp" };

    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "/file_%d.txt", i);
        snprintf(target, sizeof(target), "/file_%d_copy.txt", i);

        for (int j = 0; j < sizeof(cmds)/sizeof(cmds[0]); j++) {
            task_t *t = calloc(1, sizeof(task_t));
            strcpy(t->cmd, cmds[j]);
            strcpy(t->path, filename);
            strcpy(t->target, target);
            snprintf(t->data, sizeof(t->data), "Hello from stressfs file %d\n", i);
            enqueue(&queue, t);
        }
    }
}

// ---------- Resource Monitor ----------
void print_usage() {
    static long prev_idle = 0, prev_total = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;
    char line[256];
    fgets(line, sizeof(line), fp);
    fclose(fp);

    long user, nice, system, idle, iowait, irq, softirq, steal;
    sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", 
           &user,&nice,&system,&idle,&iowait,&irq,&softirq,&steal);
    long total = user+nice+system+idle+iowait+irq+softirq+steal;
    long diff_total = total - prev_total;
    long diff_idle = idle - prev_idle;
    double cpu_usage = 0.0;
    if (diff_total>0) cpu_usage = (1.0 - (double)diff_idle/diff_total)*100.0;
    prev_total = total;
    prev_idle = idle;

    long mem_total=0, mem_free=0;
    fp = fopen("/proc/meminfo", "r");
    if(fp) {
        char key[64]; long val;
        while(fscanf(fp, "%63s %ld kB\n", key,&val)==2){
            if(strcmp(key,"MemTotal:")==0) mem_total=val;
            if(strcmp(key,"MemAvailable:")==0) mem_free=val;
        }
        fclose(fp);
    }
    long mem_used = mem_total - mem_free;

    struct statvfs st;
    if(statvfs("/", &st)==0) {
        unsigned long disk_total = st.f_blocks * st.f_frsize/1024;
        unsigned long disk_free = st.f_bfree * st.f_frsize/1024;
        printf("[Usage] CPU: %.2f%%, Mem: %ld kB used, Disk: %lu/%lu kB\n",
               cpu_usage, mem_used, disk_total - disk_free, disk_total);
    }
}

// ---------- StressFS Command ----------
int cmd_stressfs(int argc, char **argv) {
    (void)argc; (void)argv;
    int duration_sec = 60;
    if(argc>=2) duration_sec=atoi(argv[1]);

    pthread_mutex_init(&queue.lock, NULL);
    pthread_cond_init(&queue.cond, NULL);
    for(int i=0;i<NUM_FILES;i++) pthread_mutex_init(&file_locks[i],NULL);

    pthread_t threads[NUM_THREADS];
    for(int i=0;i<NUM_THREADS;i++)
        pthread_create(&threads[i], NULL, worker, NULL);

    time_t start = time(NULL);
    while(time(NULL) - start < duration_sec) {
        generate_tasks(NUM_FILES);
        print_usage();
        sleep(1);
    }

    stop_flag=1;
    pthread_cond_broadcast(&queue.cond);
    for(int i=0;i<NUM_THREADS;i++) pthread_join(threads[i],NULL);

    pthread_mutex_destroy(&queue.lock);
    pthread_cond_destroy(&queue.cond);
    for(int i=0;i<NUM_FILES;i++) pthread_mutex_destroy(&file_locks[i]);

    printf("[+] Stress test finished\n");
    return 0;
}