#include "cmd.h"

extern int cmd_ls(int argc, char **argv);
extern int cmd_rm(int argc, char **argv);
extern int cmd_cat(int argc, char **argv);
extern int cmd_usertest(int argc, char **argv);
extern int cmd_stressfs(int argc, char **argv);
extern int cmd_echo(int argc, char **argv);
extern int cmd_cp(int argc, char **argv);
extern int cmd_fdisk(int argc, char **argv);
// 命令表
command_t command_table[] = {
    {"ls", cmd_ls},
    {"usertest", cmd_usertest},
    {"stressfs", cmd_stressfs},
    {"rm", cmd_rm},
    {"cat", cmd_cat},
    {"echo", cmd_echo},
    {"cp", cmd_cp},
    {"fdisk", cmd_fdisk},
    {NULL, NULL}  // 结束标记
};