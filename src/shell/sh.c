#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "user.h"     // 包含 my_mount, my_umount, my_open 等// command_table
#include "cmd.h"      // 命令函数声明

#define MAX_CMD 128
#define MAX_ARGS 8

void sigHandle(int sig)
{
    switch (sig) {
    case SIGINT:
        int r = my_umount("/");
        if (r < 0) {
            printf("unmount fail\n");
        }
		printf("Bye!!!\n");
        break;
    }
    exit(0); //调用exit退出程序，会被捕获该事件，从而触发进程退出处理的回调函数
}

// shell 循环
void sh_loop(void) {
    char line[MAX_CMD];
    char *argv[MAX_ARGS];

    while (1) {
        printf("$ ");
        if (!fgets(line, sizeof(line), stdin)) break;

        // 去掉换行
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        // 分词
        int argc = 0;
        char *token = strtok(line, " \t");
        while (token && argc < MAX_ARGS) {
            argv[argc++] = token;
            token = strtok(NULL, " \t");
        }
        if (argc == 0) 
            continue;

        // 查找命令表执行
        int found = 0;
        for (int i = 0; command_table[i].name != NULL; i++) {

            if (strcmp(argv[0], command_table[i].name) == 0) {
                command_table[i].func(argc, argv);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Unknown command: %s\n", argv[0]);
        }
    }
}

int init(const char *disk_path) {
    if (!disk_path) 
        return -1;

    // 1. 打开磁盘文件

    my_mount(disk_path, "/");
    // 3. 设置根目录为当前工作目录
    printf("[shell init] mounted %s to / successfully\n", disk_path);
    return 0;
}


ctx_t ctx;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <disk image path>\n", argv[0]);
        // return -1;
    }

    signal(SIGINT, sigHandle);  //Ctrl + C

    // const char *img_path = argv[1];
    const char *img_path = "../disk.img";

    // 初始化文件系统，将镜像挂载到 "/"
    if (init(img_path) < 0) {
        printf("Failed to initialize filesystem from %s\n", img_path);
        return -1;
    }

    // 进入 shell 循环
    sh_loop();

    // shell 退出后卸载根文件系统
    if (my_umount("/") < 0) {
        printf("Failed to unmount filesystem\n");
        return -1;
    }

    return 0;
}