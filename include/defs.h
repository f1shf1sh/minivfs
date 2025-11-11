#pragma once

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

/* open flags */
#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR   0x2
#define O_CREAT  0x40
#define O_APPEND 0x400

#define MAXFD 128