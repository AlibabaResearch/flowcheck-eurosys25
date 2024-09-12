#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <cstring>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

#include <vector>
#include <unordered_map>

#ifndef MEM_EXT
#define MEM_EXT


// 定义共享内存相关内容
#define RES_KEY 0x2014
#define SHM_SIZE 50LL*1024*1024*1024
#define BUFFER_SIZE (50ULL*1024*1024*1024/2048 - 1)

#define MIRROR_KEY1 0x3456
#define MIRROR_KEY2 0x5678

#define TOTAL_LENGTH 1313626112
#define TOTAL_CHUNKS 10262704 // (TOTAL_LENGTH / 128)

#if defined(__linux__)
		#include <endian.h>
		#include <byteswap.h>
#endif

#include <immintrin.h>

// extractor 数据拼接类
class Extractor{
	public:
	int shm_id;
	// int shm_id, shm_id1, shm_id2;
	char* shm_p;
	// char* shm_p1;
	// char* shm_p2;

	Extractor();
	~Extractor();
	char* get_data_ptr();
};


// bool get_shared_mem_ptr();
// char* get_data_ptr();
uint8_t* getT(int len);


#endif