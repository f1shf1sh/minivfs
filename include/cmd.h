#pragma once

typedef int (*cmd_func_t)(int argc, char **argv);

typedef struct {
    const char *name;
    cmd_func_t func;
} command_t;

extern const command_t command_table[];

int cmd_ls(int argc, char **argv);
int cmd_rm(int argc, char **argv);
int cmd_cat(int argc, char **argv);
int cmd_cp(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_touch(int argc, char **argv);
int cmd_mkdir(int argc, char **argv);
int cmd_rmdir(int argc, char **argv);
int cmd_fdisk(int argc, char **argv);
int cmd_usertest(int argc, char **argv);
int cmd_stressfs(int argc, char **argv);
int cmd_atomtest(int argc, char **argv);
