#pragma once

#include <stdio.h>

typedef int (*cmd_func_t)(int argc, char **argv);

typedef struct {
    const char *name;
    cmd_func_t func;
} command_t;

// 声明外部命令表
extern command_t command_table[];

