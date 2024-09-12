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

#include "extractor.hpp"

#define RING_NUMS 2

// #define DEBUG 0

// 互斥锁
// std::mutex mtx;


// 初始化一个全局数组，用来确定当前已经更新的具体情况
int UPDATER[CHUNK_NUMS]{};
// int TOTAL_UPDATE_NUM = 0;


// TODO: 用avx512优化这里 [done.]
// inline void copySingle(){

// }


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


// 读取inbound数据
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
        bool has_unextract_flag = false;
        uint64_t *unpack_write_index = (uint64_t*)(shm_p + 16);
        uint64_t *extractor_read_index = (uint64_t*)(shm_p + 24);
        uint64_t cur_read_index = *extractor_read_index;
        uint64_t *copy_num = (uint64_t*)(res_p + 8 + 8 * bound_flag);
        uint64_t *copy_num_ = (uint64_t*)(res_p + 8 + 8 * (1 - bound_flag));
        #ifdef DEBUG
            // printf("cur unpack index: %ld, cur ext read index: %ld , cur copy num %ld\n", (*unpack_write_index), (*extractor_read_index), (*copy_num));
        #endif
        if (((*unpack_write_index) != (*extractor_read_index)) && ((*copy_num) + (*copy_num_) != CHUNK_NUMS) && !(wait_for_next_iter)){
            *extractor_read_index = (*extractor_read_index + 1) % BUFFER_SIZE;
            has_unextract_flag = true;
        }
        // else if (wait_for_next_iter) {
        //     // 判断当前iteration是否完全处理

        // }
        else if ((*copy_num) + (*copy_num_) == CHUNK_NUMS){
            std::fill_n(UPDATER, CHUNK_NUMS, 0);
            uint64_t *iter_num = (uint64_t*)(res_p);
            
            // wait_for_next_iter = false; // 这玩意还最好整成共享变量

            // copy_num 由后续的updater进行清空; 或者也可以根据copy_num 是否被清空来判断是否进入下一个iteration
            if ((*iter_num) != (local_iter_num))
            {
                // iter 更新，代表数据读取完毕，此时进行数据覆写
                #ifdef DEBUG
                    printf("iteration extract done. cur bound %d\n", bound_flag);
                #endif
                wait_for_next_iter = false;
                local_iter_num += 1;
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

        // TODO: 这里的逻辑需要改一改， 加一个iteration数据的判断，避免在mirror双流的同时读取到下一个iteration的数据
        if (has_unextract_flag){
            // 仍有未拼接的已解包数据
            uint16_t *pac_len = (uint16_t*)(shm_p + 2048 + (cur_read_index * 2048));
            uint64_t *payload_index = (uint64_t*)(shm_p + 2048 + (cur_read_index * 2048) + 2 + (*pac_len));
            uint64_t *data_index = (uint64_t*)(shm_p + 2048 + (cur_read_index * 2048) + 2 + (*pac_len) + 8);
            // #ifdef DEBUG
                // printf("enter extract logic!\n");
            // #endif
            // 如果payload_index =0 说明是无效包
            if (*payload_index == 0){
                continue;
            }

            // 判断当前位置数据是否已经解过包
            // mtx.lock();
            if ((*data_index)/128 > CHUNK_NUMS - 1){
                printf("warn data index overflow %" PRIu64 ", origin data index %" PRIu64  ", cur_read_index %" PRIu64 "\n", (*data_index)/128, (*data_index), cur_read_index);
            }
            if (UPDATER[(*data_index)/128] == 0){
                // if ((*data_index)/128 > CHUNK_NUMS - 1){
                //     printf("warn data index overflow %ld\n", (*data_index)/128);
                // }
                UPDATER[(*data_index)/128] = 1;
                #ifdef DEBUG
                    // printf("origin index: %ld start index been processed: %ld\n", (*data_index), (*data_index)/128);
                    // printf("payload index: %ld, cur read index %ld, cur total index %ld, cur write length %ld\n", (*payload_index), cur_read_index, (2048 + (cur_read_index * 2048) + 2 + (*payload_index)), (*pac_len - *payload_index - 4));
                #endif

                // 根据index进行对应位置的copy
                // cpy 逻辑有问题！ 理论上只应当cpy进去真实数据部分而不应当cpy进flag
                // 另外还有个问题，有些数据包是无效包不用copy: 添加对payload_index 的判断： 如果是0 则说明是无效包
                // memcpy(res_p + 2048 + (*data_index * 4), shm_p + 2048 + (cur_read_index * 2048) + 2 + (*payload_index), (*pac_len - *payload_index - 4));
                // copyData(res_p + 2048 + (*data_index * 4), shm_p + 2048 + (cur_read_index * 2048) + 2 + (*payload_index), 4, (*pac_len - *payload_index - 4));
                copyDataAVX512((uint32_t*)(res_p + 2048 + (*data_index * 4)), (uint64_t*)(shm_p + 2048 + (cur_read_index * 2048) + 2 + (*payload_index)), (*pac_len - *payload_index - 4));

                // 更新copy完成总数
                uint64_t *copy_num = (uint64_t*)(res_p + 8 + 8 * bound_flag);
                *copy_num += 1;
                // pac_get_num += 1;
                if (*copy_num == CHUNK_NUMS / RING_NUMS * (RING_NUMS - 1) ){
                    wait_for_next_iter = true;
                    #ifdef DEBUG
                        printf("cur mirror extract done. cur bound %d\n", bound_flag);
                    #endif
                }
                // TOTAL_UPDATE_NUM += 1;
                // copy_num == 7813 标志已全部copy完成；等待updater提取。 若 copy_num 否: 标志copy仍未完成，还需要进行copy
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
                    // uint64_t *copy_num_ = (uint64_t*)(res_p + 8 + 8 * (1 - bound_flag));
                    // printf("processed one pac, current process num %ld\n", (*copy_num + *copy_num_));
                #endif
            }
            // mtx.unlock();
            
        }
        else{
                struct timespec ts;
                double sleepTime = 0.01;
                // 将浮点数时间转换为秒和纳秒
                ts.tv_sec = (time_t)sleepTime;
                ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒

                // 调用 nanosleep 休眠
                nanosleep(&ts, NULL);
                // printf("=====CURRENT SLEEPING=====\n");
                // printf("cur read ind: %lld cur write ind: %lld\n", (*read_index), (*write_index));

                #ifdef DEBUG
                    // uint64_t *exact_index = (uint64_t*)(shm_p + 16);
                    // uint64_t *read_index = (uint64_t*)(shm_p + 8);
                    // uint64_t *write_index = (uint64_t*)(shm_p);
                    // if ((*write_index > 10000000)){
                    //     printf("exact_index %ld, read_index %ld, write_index %ld, from bound %d\n", (*exact_index), (*read_index), (*write_index), bound_flag);
                    // }
                #endif

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