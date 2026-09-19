#include "cmd.h"
#include <stddef.h>

const command_t command_table[] = {
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"cp", cmd_cp},
    {"echo", cmd_echo},
    {"rm", cmd_rm},
    {"touch", cmd_touch},
    {"mkdir", cmd_mkdir},
    {"rmdir", cmd_rmdir},
    {"fdisk", cmd_fdisk},
    {"usertest", cmd_usertest},
    {"stressfs", cmd_stressfs},
    {"atomtest", cmd_atomtest},
    {NULL, NULL}
};
