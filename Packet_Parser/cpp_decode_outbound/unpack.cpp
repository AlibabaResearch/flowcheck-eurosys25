#include "unpack.hpp"
#include <vector>
#include <unordered_map>
#include <thread>

// 多QP时，定义shm_key 数组
int SHM_KEY_GROUP[] = {0x5678}; 


void start_state_machine(int i){
    // 初始化状态机类并运行
    PackageProcessor pac_processor(SHM_KEY_GROUP[i]);
    pac_processor.run();
}


int main(void){
    std::vector<std::thread> threads;
    // 启动多核心读取逻辑，每个thread，传入两个共享内存id
    // int lcore_nums = QP_NUM;
    int lcore_nums = 1;
    for (int i = 0; i < lcore_nums; ++i){
        threads.emplace_back(start_state_machine, i);
    }

    for (auto& th: threads){
        th.join();
    }

    return 0;
}