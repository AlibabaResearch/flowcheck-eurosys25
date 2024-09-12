#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>

#ifndef EXT
#define EXT

#define SHM_KEY 0x3456
#define SHM_KEY2 0x5678
#define SHM_SIZE 50LL*1024*1024*1024
#define BUFFER_SIZE (50ULL*1024*1024*1024/2048 - 1)
#define FINAL_KEY 0x2014
#define TOTAL_LENGTH 1313626112
#define CHUNK_NUMS 10262704 // (TOTAL_LENGTH / 128 向下取整 + 1)
#define MTU 1024
#define DEBUG 0

#if defined(__APPLE__)
#include <machine/endian.h>
#endif

#if defined(__linux__)
#  include <endian.h>
#include <byteswap.h>
// #  define be16toh(x) betoh16(x)
// #  define be32toh(x) betoh32(x)
// #  define be64toh(x) betoh64(x)
#endif



#endif