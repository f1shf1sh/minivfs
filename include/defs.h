#pragma once

#include <fcntl.h>

// disk layer
#define BSIZE (1 << 12) // size of block
#define BCOUNTS (1 << 16) // total blocks
#define DSIZE ((BSIZE) * (BCOUNTS)) // size of disk

// other ops
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// bitmap ops
#define BIT_SET(map, n)   ((map)[(n)/8] |=  (1u << ((n) % 8)))
#define BIT_CLR(map, n)   ((map)[(n)/8] &= ~(1u << ((n) % 8)))
#define BIT_TST(map, n)   (((map)[(n)/8] >> ((n) % 8)) & 1u)

#define FILENAME_MAX_LEN 60

#define MY_PATH_MAX 4096
#define MAXFD 128
