#include "cmd.h"
#include "user.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD 1024
#define MAX_ARGS 32

static volatile sig_atomic_t interrupted;

static void handle_sigint(int signal_number) {
    (void)signal_number;
    interrupted = 1;
}

static int sh_loop(void) {
    char *line = NULL;
    size_t capacity = 0;
    int result = 0;
    int interactive = isatty(STDIN_FILENO);
    while (!interrupted) {
        if (interactive) {
            fputs("$ ", stdout);
            fflush(stdout);
        }
        errno = 0;
        ssize_t length = getline(&line, &capacity, stdin);
        if (length < 0) {
            if (ferror(stdin) && !interrupted) {
                perror("shell: input");
                result = 1;
            }
            break;
        }
        if (interrupted)
            break;
        if (length > MAX_CMD) {
            fprintf(stderr, "shell: command exceeds %u bytes\n", MAX_CMD);
            result = 1;
            continue;
        }

        char *argv[MAX_ARGS + 1];
        int argc = 0;
        char *saveptr;
        char *token = strtok_r(line, " \t\r\n", &saveptr);
        while (token && argc < MAX_ARGS) {
            argv[argc++] = token;
            token = strtok_r(NULL, " \t\r\n", &saveptr);
        }
        argv[argc] = NULL;
        if (token) {
            fprintf(stderr, "shell: too many arguments\n");
            result = 1;
            continue;
        }
        if (!argc)
            continue;
        if (strcmp(argv[0], "exit") == 0) {
            if (argc == 1)
                break;
            fprintf(stderr, "Usage: exit\n");
            result = 1;
            continue;
        }
        if (strcmp(argv[0], "sync") == 0) {
            if (argc != 1) {
                fprintf(stderr, "Usage: sync\n");
                result = 1;
            } else if (my_sync() < 0) {
                perror("sync");
                result = 1;
            }
            continue;
        }

        const command_t *command = command_table;
        while (command->name && strcmp(command->name, argv[0]) != 0)
            command++;
        if (!command->name) {
            fprintf(stderr, "Unknown command: %s\n", argv[0]);
            result = 1;
        } else if (command->func(argc, argv) < 0) {
            result = 1;
        }
    }
    free(line);
    return interrupted ? 130 : result;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image path>\n", argv[0]);
        return 1;
    }
    struct sigaction action = {0};
    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0) {
        perror("shell: sigaction");
        return 1;
    }
    if (my_mount(argv[1], "/") < 0) {
        perror("shell: mount");
        return 1;
    }
    /* Commands join their workers before returning, so teardown runs alone. */
    int result = sh_loop();
    if (my_umount("/") < 0) {
        perror("shell: unmount");
        result = 1;
    }
    if (fflush(stdout) == EOF)
        result = 1;
    return result;
}
