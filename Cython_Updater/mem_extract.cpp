#include "mem_extract.hpp"

#include <time.h>

// char* shm_p = nullptr;

// bool get_shared_mem_ptr(){
//     int shm_id = shmget(RES_KEY, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
//     if (shm_id < 0){
//             perror("shmget");
//             return false;
//         }
//     shm_p = (char *)shmat(shm_id, NULL, 0);
//     if((void *)shm_p == (void *)-1){
//             perror("shmat()");
//             return false;
//         }
//     return true;
// }

Extractor::Extractor(){
    shm_id = shmget(RES_KEY, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
    if (shm_id < 0){
            perror("shmget");
            exit(1);
        }
    shm_p = (char *)shmat(shm_id, NULL, 0);
    if((void *)shm_p == (void *)-1){
            perror("shmat()");
            exit(1);
        }


    // 添加两个mirror dump 共享内存的接入
    // shm_id1 = shmget(MIRROR_KEY1, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
    // if (shm_id1 < 0){
    //         perror("shmget");
    //         exit(1);
    //     }
    // shm_p1 = (char *)shmat(shm_id1, NULL, 0);
    // if((void *)shm_p1 == (void *)-1){
    //         perror("shmat()");
    //         exit(1);
    //     }
    
    // shm_id2 = shmget(MIRROR_KEY2, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
    // if (shm_id2 < 0){
    //         perror("shmget");
    //         exit(1);
    //     }
    // shm_p2 = (char *)shmat(shm_id2, NULL, 0);
    // if((void *)shm_p2 == (void *)-1){
    //         perror("shmat()");
    //         exit(1);
    //     }
}

Extractor::~Extractor(){
    // 销毁共享内存
    // shmctl(shm_id, IPC_RMID, NULL);
}

char* Extractor::get_data_ptr(){
    if (shm_p == nullptr){
        perror("Shared mem not init");
        exit(1);
    }

    // uint64_t value;
    // memcpy(&value, shm_p, sizeof(uint64_t));
    uint64_t *value = (uint64_t*)(shm_p + 8);
    uint64_t *value_ = (uint64_t*)(shm_p + 16);
    // printf("cur value: %ld\n", (*value));

    // 阻塞直到flag更新了为止
    while ((*value + *value_) < TOTAL_CHUNKS){
        // memcpy(&value, shm_p, sizeof(uint64_t));

        // value = (uint64_t*)(shm_p + 8);
        // value_ = (uint64_t*)(shm_p + 16);

        // printf("cur value: %ld\n", (*value));

        // debug
        // exit(1);

        struct timespec ts;
        double sleepTime = 0.01;
        // 将浮点数时间转换为秒和纳秒
        ts.tv_sec = (time_t)sleepTime;
        ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒
        // 调用 nanosleep 休眠
        nanosleep(&ts, NULL);

        // 在这个期间进行 两个mirror流 dump 内存的清零操作        
        // uint64_t *extract_index1 = (uint64_t*)(shm_p1 + 24); // 初始0
        // uint64_t *clean_index1 = (uint64_t*)(shm_p1 + 32); // 初始0 

        // if ((((*extract_index1)) > (*clean_index1 + 100) % BUFFER_SIZE)){
        //     memset(shm_p1 + 2048 + (*clean_index1) * 2048, 0, 20480);
        //     *clean_index1 = (*clean_index1 + 10) % BUFFER_SIZE;
        // }
        

        // uint64_t *extract_index2 = (uint64_t*)(shm_p2 + 24);
        // uint64_t *clean_index2 = (uint64_t*)(shm_p2 + 32);

        // if ((((*extract_index2)) > (*clean_index2 + 100) % BUFFER_SIZE)){
        //     memset(shm_p2 + 2048 + (*clean_index2) * 2048, 0, 20480);
        //     *clean_index2 = (*clean_index2 + 10) % BUFFER_SIZE;
        // }

        // 操作执行完毕后再次查询最新的 copy_num 值
        value = (uint64_t*)(shm_p + 8);
        value_ = (uint64_t*)(shm_p + 16);

    }


    // 读取成功， 写回value至0, 更新iteration num， 等待下一个iteration
    *value = 0;
    *value_ = 0;
    uint64_t *iter_num = (uint64_t*)(shm_p);
    *iter_num += 1;

    return shm_p + 2048;

}

uint8_t* getT( uint64_t len)
{
	if(len%4 != 0) return NULL;
	uint8_t* ptr = (uint8_t*)calloc(len,sizeof(uint8_t));
    // for(int64_t i = 0; i < (len>>2); i++){
        // ((_Float32*)ptr)[i] = (_Float32)rand();
    // }
	return ptr;
}

int main(){


    
}