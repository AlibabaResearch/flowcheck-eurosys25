/*
    extractor 启动主程序, 启动多个物理核心进行copy操作
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <thread>
#include <iostream>
#include <vector>
#include <atomic>
#include <mutex>
#include <immintrin.h>
#include <time.h>
#include <algorithm>
#include <cinttypes>
#include <chrono>
#include <iostream>

#include "extractor.hpp"

#define RING_NUMS 2

// #define DEBUG 0

// 互斥锁
// std::mutex mtx;


// 初始化一个全局数组，用来确定当前已经更新的具体情况
int UPDATER[CHUNK_NUMS]{};
int UPDATER_FLAG = 0;
// int TOTAL_UPDATE_NUM = 0;

auto start_time = std::chrono::high_resolution_clock::now();
auto end_time = std::chrono::high_resolution_clock::now();


inline void copyDataAVX512(uint32_t* dst, uint64_t* src, size_t len){
    uint32_t i = 0;
    if(len >= 64){
        for(i = 0; i < (len>>3); i += 8){
            __m512i vec64 = _mm512_loadu_si512(src + i);

            __m256i vec32 =  _mm512_cvtepi64_epi32(vec64);

            _mm256_storeu_si256((__m256i*)(dst + i), vec32);

        }
    }
    // if(len % 64 != 0){
    //     copySingle(src + i * 8,len%64);
    // }
}


// void copyData(char* p, char* q, size_t blockSize, size_t totalBlocks) {
//     // blockSize 是每个数据块有效数据的大小（4 bytes）
//     // totalBlocks 是数据块的总数量（这里是1024）

//     // const char* qEnd = q + totalBlocks; // 指向q末尾的指针
//     size_t already_get_num = 0;
//     while (already_get_num < totalBlocks) {
//         memcpy(p, q, blockSize); p += blockSize; q += blockSize;
//         q += blockSize; // 跳过flag部分
//         already_get_num += 2*blockSize;
//     }
// }

bool is_current_round(const char* ptr, int round_num){
    uint8_t *round_flag = (uint8_t*)(ptr + 21);
    if ((*round_flag) != round_num){
        #ifdef DEBUG
            printf("round tag wrong, cur round %d, except flag %d\n", (*round_flag), round_num);
        #endif
        return false;
    }
    return true;
}


void exract_data(int shm_id, int res_id, int bound_flag){
    // debug
    // #ifdef DEBUG
    //     if (bound_flag == 1){
    //         struct timespec ts;
    //         double sleepTime = 10;
    //         // 将浮点数时间转换为秒和纳秒
    //         ts.tv_sec = (time_t)sleepTime;
    //         ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒

    //         // 调用 nanosleep 休眠
    //         nanosleep(&ts, NULL);
    //         printf("Sleep done, now start thread2\n");
    //     }
    // #endif

    uint64_t local_iter_num = 0;

    int timing_flag = 0;

    // uint8_t round_num = 1;


    //获取共享内存指针
    char *shm_p = (char *)shmat(shm_id, NULL, 0);
	if((void *)shm_p == (void *)-1){
		perror("shmat()");
		exit(1);
	}
	// memset(shm_p, 0, 1024*1024*1024);
    char *res_p = (char *)shmat(res_id, NULL, 0);
	if((void *)res_p == (void *)-1){
		perror("shmat()");
		exit(1);
	}
    if (bound_flag == 0){
        // 初始化res 共享内存区域
        memset(res_p, 0, SHM_SIZE);
        printf("initial done.\n");
    }
    // else {
    //     struct timespec ts;
    //     double sleepTime = 10;
    //     // 将浮点数时间转换为秒和纳秒
    //     ts.tv_sec = (time_t)sleepTime;
    //     ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒

    //     // 调用 nanosleep 休眠
    //     nanosleep(&ts, NULL);
    //     printf("Sleep done, now start thread2\n");
    // }

    // uint64_t pac_get_num = 0;
    bool wait_for_next_iter = false;


    while (true){

        uint64_t *unpack_write_index = (uint64_t*)(shm_p + 16);
        uint64_t *extractor_read_index = (uint64_t*)(shm_p + 24);

        if (((*unpack_write_index) != (*extractor_read_index))){
            if (!wait_for_next_iter){

                uint16_t *pac_len = (uint16_t*)(shm_p + 2048 + (*extractor_read_index * 2048));
                uint64_t *payload_index = (uint64_t*)(shm_p + 2048 + (*extractor_read_index * 2048) + 2032);
                uint64_t *data_index = (uint64_t*)(shm_p + 2048 + (*extractor_read_index * 2048) + 2040);

                if (*payload_index == 0){
                    *extractor_read_index = (*extractor_read_index + 1) % BUFFER_SIZE;
                    continue;
                }

                if ((*data_index)/128 > CHUNK_NUMS - 1){
                    printf("warn data index overflow %" PRIu64 ", origin data index %" PRIu64  ", cur_read_index %" PRIu64 " cur unpack index %" PRIu64 "\n", (*data_index)/128, (*data_index), *extractor_read_index, *unpack_write_index);
                }

                if (UPDATER[(*data_index)/128] == 0){
                    UPDATER[(*data_index)/128] = 1;

                    copyDataAVX512((uint32_t*)(res_p + 2048 + (*data_index * 4)), (uint64_t*)(shm_p + 2048 + (*extractor_read_index * 2048) + 2 + (*payload_index)), (*pac_len - *payload_index - 4));

                    // 更新copy完成总数
                    uint64_t *copy_num = (uint64_t*)(res_p + 8 + 8 * bound_flag);
                    *copy_num += 1;
                    // pac_get_num += 1;
                    // fix the stop condition
                    if (*copy_num == CHUNK_NUMS / RING_NUMS * (RING_NUMS - 1) ){
                        wait_for_next_iter = true;
                        #ifdef DEBUG
                            printf("cur mirror extract done. cur bound %d\n", bound_flag);
                        #endif

                        if (timing_flag != 1){
                            start_time = std::chrono::high_resolution_clock::now();
                            timing_flag = 1;
                        }
                        else{
                            end_time = std::chrono::high_resolution_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                            #ifdef DEBUG
                                // printf("iteration process duration: %ld", duration.count());
                                std::cout << "iteration process time taken: " << duration.count() << " microseconds" << std::endl;
                            #endif
                            timing_flag = 2;
                        }

                    }

                    #ifdef DEBUG
                        if (*copy_num % 100000 == 0){
                            uint64_t *copy_num_ = (uint64_t*)(res_p + 8 + 8 * (1 - bound_flag));
                            printf("processed one pac, current process num %ld\n", (*copy_num + *copy_num_));
                            uint64_t *exact_index = (uint64_t*)(shm_p + 24);
                            uint64_t *read_index = (uint64_t*)(shm_p + 16);
                            uint64_t *al_write_index = (uint64_t*)(shm_p + 8);
                            uint64_t *write_index = (uint64_t*)(shm_p);
                            printf("exact_index %ld, read_index %ld, already write_index %ld, write index %ld, from bound %d\n", (*exact_index), (*read_index), (*al_write_index), (*write_index), bound_flag);
                        }
                    #endif
                }else {
                    struct timespec ts;
                    double sleepTime = 0.01;
                    // 将浮点数时间转换为秒和纳秒
                    ts.tv_sec = (time_t)sleepTime;
                    ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒
                    // 调用 nanosleep 休眠
                    nanosleep(&ts, NULL);
                }

                *extractor_read_index = (*extractor_read_index + 1) % BUFFER_SIZE;
   
            }else {

                uint64_t *iter_num = (uint64_t*)(res_p);
                if ((*iter_num) != (local_iter_num))
                {
                    // iter 更新，代表数据读取完毕，此时进行数据覆写
                    #ifdef DEBUG
                        printf("iteration extract done. cur bound %d\n", bound_flag);
                    #endif
                    wait_for_next_iter = false;
                    local_iter_num += 1;
                    
                    // 置零
                    if (bound_flag == 0){
                        std::fill_n(UPDATER, CHUNK_NUMS, 0);
                        UPDATER_FLAG = 1;
                    }
                    else{
                        // wait for fill zero
                        while(UPDATER_FLAG != 1){
                            struct timespec ts;
                            double sleepTime = 0.01;
                            // 将浮点数时间转换为秒和纳秒
                            ts.tv_sec = (time_t)sleepTime;
                            ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒
                            // 调用 nanosleep 休眠
                            nanosleep(&ts, NULL);
                        }
                        UPDATER_FLAG = 2;
                    }
                }
                else {
                    struct timespec ts;
                    double sleepTime = 0.01;
                    // 将浮点数时间转换为秒和纳秒
                    ts.tv_sec = (time_t)sleepTime;
                    ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒
                    // 调用 nanosleep 休眠
                    nanosleep(&ts, NULL);
                }
            }
        }
        else {
            struct timespec ts;
            double sleepTime = 0.01;
            // 将浮点数时间转换为秒和纳秒
            ts.tv_sec = (time_t)sleepTime;
            ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒
            // 调用 nanosleep 休眠
            nanosleep(&ts, NULL);
        }

    }


}


int main(int argc, char ** argv){
    // 在主进程中获取共享内存id
    int shm_id, res_id, shm_id2;
    int lcore_nums, i;

    shm_id = shmget(SHM_KEY, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
    if (shm_id < 0){
        perror("shmget:shm1");
        exit(1);
    }

    shm_id2 = shmget(SHM_KEY2, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
    if (shm_id2 < 0){
        perror("shmget:shm2");
        exit(1);
    }

    res_id = shmget(FINAL_KEY, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
    if (res_id < 0){
        perror("shmget:final");
        exit(1);
    }

    std::vector<std::thread> threads;
    // 启动多核心读取逻辑，每个thread，传入两个共享内存id
    lcore_nums = 2;
    for (i = 0; i < lcore_nums/2; ++i){
        threads.emplace_back(exract_data, shm_id, res_id, 0);
    }
    for (i = 0; i < lcore_nums/2; ++i){
        threads.emplace_back(exract_data, shm_id2, res_id, 1);
    }

    for (auto& th: threads){
        th.join();
    }

    // debug 最终验证
    for (int i = 0; i < CHUNK_NUMS; ++i){
        if (UPDATER[i] != 1){
            printf("non 1 index %d corr data index %ld \n", i, (long)(i*128));
        }
    }

    return 0;

}