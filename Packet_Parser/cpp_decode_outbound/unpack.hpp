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
#include <cinttypes>
#include <chrono>
#include <iostream>

#include <time.h>


#ifndef DEC
#define DEC
#define MTU 1024
// #define MACMASK 0xffffffffffff0000 // 8 bytes ; 6 + 2
#define MACMASK 0x0000ffffffffffff
#define RING_NUMS 8
#define QP_NUM 1

#define DATA_LENGTH 1313626112
#define BC_LENGTH 0 
// Broadcast阶段需要收集的网络包总长度 （实际参数量 * 8）
#define DEBUG 0


// 定义共享内存参数
#define SHM_SIZE 50LL*1024*1024*1024
#define BUFFER_SIZE (50ULL*1024*1024*1024/2048 - 1)


#if defined(__APPLE__)
#include <machine/endian.h>
#endif

#if defined(__linux__)
#  include <endian.h>
#include <byteswap.h>
#endif

// outbound情况下需要解析dstsrc
#pragma pack(2)
struct _pkt_s{ // 从Eth层到IB层的解析： vaddr可能是空（取决于后续有没有RETH层）
	// uint8_t  dstMac[4];
    uint64_t dstMac;
	// uint16_t SrcMac;
	//real src mac = ntohll(srcMac&0xffffffffffff0000)
	// uint64_t srcMac;
    uint8_t srcMac[4];
	uint16_t ethType;
	// uint8_t padding1[42];
	uint8_t padding0[4];
	uint16_t length;
	uint8_t padding1[36];
	// uint16_t sport;
	uint16_t dport;
	uint8_t padding2[4];
	uint8_t ops;
	uint8_t padding3[3];
	uint32_t dqpn; //real_dqpn = ntohl(dqpn)
	uint32_t psn;// real_psn = ntohl(psn&0xffffff00)
	// uint8_t padding3;
	uint64_t vaddr;
    uint32_t rmt_key;
    uint32_t dma_length;
};
typedef struct _pkt_s pkt_s;
pkt_s *pkt_h;

#pragma pack()

struct _dh_result{
	int8_t flag; // flag 含义： 表示payload从哪里开始
	uint64_t index; // index 含义：在原始总数据中的index位置
};
typedef struct _dh_result dh_result;

typedef enum {
ETH = 14,
PADDING = 48,
BTH = 12,
RETH = 16,
IMMDT = 4
} pSize;

// uint64_t MatchDstMac = 0xF8589FA1420C0000; // 需要判断的src mac地址 0c42a19f58f9 注意这里要反着写  (如果dpdk 逐 bytes copy的话是 F9589FA1420C0000)
uint64_t MatchDstMac = 0xF8589FA1420C; // 需要判断的src mac地址 0c42a19f58f9 注意这里要反着写  (如果dpdk 逐 bytes copy的话是 F9589FA1420C0000)

// Return -1 跳过非目标包
//highMac和lowMac需要
//目标为Intel CPU，host格式为小端，和网络相反
// dh_result decode_hex(uint8_t* payload, uint32_t len);

auto start_time = std::chrono::high_resolution_clock::now();
auto end_time = std::chrono::high_resolution_clock::now();

// 状态机类
class PackageProcessor {
public:
    int shm_key;
    long long bc_gather_num;
    uint8_t* dest;
    dh_result result;
    int iter_num;

    // 根据length划分长度变量定义
    int trans_num, last_trans_size, modal_chunk_size, last_chunk_size;
    int cur_trans_num, cur_chunk_num, cur_pac_num;

    // 第二次iteration开始后的操作数相关定义
    uint64_t* write_to_data_index; // 每次RDMA write操作到实际data index的转换数组 ; RDMA 操作数 16783360 16785408 16789504
    uint64_t RDMA_length[4] = {16783360, 16785408, 16789504, 105027584};
    int RDMA_nums;
    int trans_num1, trans_num2, trans_num3, trans_num4; // 分别对应四种不同RDMA length的trans 数
    int last_trans_size1, last_trans_size2, last_trans_size3, last_trans_size4;
    int modal_chunk_size1, modal_chunk_size2, modal_chunk_size3, modal_chunk_size4;
    int last_chunk_size1, last_chunk_size2, last_chunk_size3, last_chunk_size4;
    int chosen_trans_num, chosen_last_trans_size, chosen_modal_chunk_size, chosen_last_chunk_size;
    int cur_write_num;

    int cur_bc_length;

    int round_num;

    int timing_flag;

    // int unpack_num;

    PackageProcessor(int key)
        :  bc_gather_num(0)
    {
        shm_key = key;
        dest = new uint8_t[2 * MTU];

        // 初始化 length计算
        trans_num = DATA_LENGTH / (8192 * RING_NUMS * QP_NUM);
        if (trans_num < 1){
            // 数据能够在一个trans内传输完毕
            
        }
        else{
            // 计算最后一个chunk的 size
            last_trans_size = DATA_LENGTH % (8192 * RING_NUMS * QP_NUM);
            if (last_trans_size < 1024){
                // 最后一个WRITE IMMDT操作
                
            }
            else if (last_trans_size < 8192){
                // 整个包一起发送，不分chunks
                modal_chunk_size = (last_trans_size * 8 / RING_NUMS / 8192) * 8192; // 40960
                last_chunk_size = last_trans_size * 8 - modal_chunk_size * (RING_NUMS - 1); // 
            }
            else{
                modal_chunk_size = (last_trans_size * 8 / RING_NUMS / 8192 + 1) * 8192;
                last_chunk_size = last_trans_size * 8 - modal_chunk_size * (RING_NUMS - 1);
            }
        }


        cur_trans_num = 0;
        cur_chunk_num = 0;
        cur_pac_num = 0;
        iter_num = 0;
        cur_write_num = 0;
        round_num = 1;
        timing_flag = 0;

        // 接收第一个iteration的相关变量初始化
        chosen_trans_num = trans_num;
        chosen_last_trans_size = last_trans_size;
        chosen_modal_chunk_size = modal_chunk_size;
        chosen_last_chunk_size = last_chunk_size;
        cur_bc_length = BC_LENGTH;

        // 计算第二个iteration之后的对应起始index
        RDMA_nums = (DATA_LENGTH - RDMA_length[3]) / (RDMA_length[0] + RDMA_length[1] + RDMA_length[2]) * 3 + 1;
        write_to_data_index = new uint64_t[RDMA_nums];
        uint64_t cur_index_pos = DATA_LENGTH;
        for(int i=0;i<RDMA_nums - 1;++i){
            cur_index_pos -= RDMA_length[i % 3];
            write_to_data_index[i] = cur_index_pos;
        }
        write_to_data_index[RDMA_nums-1] = 0;

        trans_num1 = RDMA_length[0] / (8192 * RING_NUMS * QP_NUM);
        if (trans_num1 < 1){
            // 数据能够在一个trans内传输完毕
            
        }
        else{
            // 计算最后一个chunk的 size
            last_trans_size1 = RDMA_length[0] % (8192 * RING_NUMS * QP_NUM); // 16960
            if (last_trans_size1 < 1024){
                // 最后一个WRITE IMMDT操作
                
            }
            else if (last_trans_size1 <= 8192){
                // 整个包一起发送，不分chunks
                modal_chunk_size1 = (last_trans_size1 * 8 / RING_NUMS / 8192) * 8192; // 40960
                last_chunk_size1 = last_trans_size1 * 8 - modal_chunk_size1 * (RING_NUMS - 1); // 
            }
            else{
                modal_chunk_size1 = (last_trans_size1 * 8 / RING_NUMS / 8192 + 1) * 8192; // 40960
                last_chunk_size1 = last_trans_size1 * 8 - modal_chunk_size1 * (RING_NUMS - 1); // 
            }
        }

        trans_num2 = RDMA_length[1] / (8192 * RING_NUMS * QP_NUM);
        if (trans_num2 < 1){
            // 数据能够在一个trans内传输完毕
            
        }
        else{
            // 计算最后一个chunk的 size
            last_trans_size2 = RDMA_length[1] % (8192 * RING_NUMS * QP_NUM); // 16960
            if (last_trans_size2 < 1024){
                // 最后一个WRITE IMMDT操作
                
            }
            else if (last_trans_size2 <= 8192){
                // 整个包一起发送，不分chunks
                modal_chunk_size2 = (last_trans_size2 * 8 / RING_NUMS / 8192) * 8192; // 40960
                last_chunk_size2 = last_trans_size2 * 8 - modal_chunk_size2 * (RING_NUMS - 1); // 
            }
            else{
                modal_chunk_size2 = (last_trans_size2 * 8 / RING_NUMS / 8192 + 1) * 8192; // 40960
                last_chunk_size2 = last_trans_size2 * 8 - modal_chunk_size2 * (RING_NUMS - 1); // 
            }
        }

        trans_num3 = RDMA_length[2] / (8192 * RING_NUMS * QP_NUM);
        if (trans_num3 < 1){
            // 数据能够在一个trans内传输完毕
            
        }
        else{
            // 计算最后一个chunk的 size
            last_trans_size3 = RDMA_length[2] % (8192 * RING_NUMS * QP_NUM); // 16960
            if (last_trans_size3 < 1024){
                // 最后一个WRITE IMMDT操作
                
            }
            else if (last_trans_size3 <= 8192){
                // 整个包一起发送，不分chunks
                modal_chunk_size3 = (last_trans_size3 * 8 / RING_NUMS / 8192) * 8192; // 40960
                last_chunk_size3 = last_trans_size3 * 8 - modal_chunk_size3 * (RING_NUMS - 1); // 
            }
            else{
                modal_chunk_size3 = (last_trans_size3 * 8 / RING_NUMS / 8192) * 8192; // 40960
                last_chunk_size3 = last_trans_size3 * 8 - modal_chunk_size3 * (RING_NUMS - 1); // 
            }
        }

        trans_num4 = RDMA_length[3] / (8192 * RING_NUMS * QP_NUM);
        if (trans_num4 < 1){
            // 数据能够在一个trans内传输完毕
            
        }
        else{
            // 计算最后一个chunk的 size
            last_trans_size4 = RDMA_length[3] % (8192 * RING_NUMS * QP_NUM); // 16960
            if (last_trans_size4 < 1024){
                // 最后一个WRITE IMMDT操作
                
            }
            else if (last_trans_size4 <= 8192){
                // 整个包一起发送，不分chunks
                modal_chunk_size4 = (last_trans_size4 * 8 / RING_NUMS / 8192) * 8192; // 40960
                last_chunk_size4 = last_trans_size4 * 8 - modal_chunk_size4 * (RING_NUMS - 1); // 
            }
            else{
                modal_chunk_size4 = (last_trans_size4 * 8 / RING_NUMS / 8192 + 1) * 8192; // 40960
                last_chunk_size4 = last_trans_size4 * 8 - modal_chunk_size4 * (RING_NUMS - 1); // 
            }
        }


        // debug for class init stage
        #ifdef DEBUG
            printf("last_trans_size %d, modal_chunk_size %d, last_chunk_size %d\n", last_trans_size, modal_chunk_size, last_chunk_size);
            printf("trans_num: %d %d %d %d\n", trans_num1, trans_num2, trans_num3, trans_num4);
            printf("last_trans_size: %d %d %d %d\n", last_trans_size1, last_trans_size2, last_trans_size3, last_trans_size4);
            printf("modal_chunk_size: %d %d %d %d\n", modal_chunk_size1, modal_chunk_size2, modal_chunk_size3, modal_chunk_size4);
            printf("last_chunk_size: %d %d %d %d\n", last_chunk_size1, last_chunk_size2, last_chunk_size3, last_chunk_size4);
            printf("RDMA_nums %d\n", RDMA_nums);
            for (int i=0;i<RDMA_nums; ++i){
                printf("%ld,", write_to_data_index[i]);
            }
            printf("\n");
        #endif
    }

    // void transition(uint8_t* payload, int pac_seq, uint16* length){
    //     decode_hex(payload, )
    // }

    ~PackageProcessor(){
        delete[] dest;
        delete[] write_to_data_index;
    }


    void decode_hex(uint8_t* payload, uint32_t len, dh_result* result){
        uint32_t cur_pPr = 0;
        pkt_h = (pkt_s*)payload;
        // int8_t flag = 0;
        // uint16_t dma_len = 0;


        int chunk_id;

        if(
				((pkt_h->dstMac&MACMASK)!= MatchDstMac) || 
				(pkt_h->ethType!=0xdd86) || 
				(pkt_h->dport != 0xb712)
			){
			#ifdef DEBUG
				// printf("%lX\n", pkt_h->dstMac);
				// printf("===========Eth Wrong============\n");
				// printf("%lX\n", pkt_h->dstMac&MACMASK);
				// printf("%X", pkt_h->ethType);
				// printf("%X", pkt_h->dport);
			#endif
            result->flag = -1;
			return ;
		}

        switch (pkt_h->ops)
		{
		case 9://WRITE IMMEDIATE
			cur_pPr = 78;
			if(bc_gather_num != BC_LENGTH){
				bc_gather_num += len - cur_pPr - 4;
				result->flag = -1;
				#ifdef DEBUG
					printf("==============BROAD PACKET===========\n");
				#endif
			}else{
				result->flag = cur_pPr;

                // index 计算
                cur_pac_num += 1;

                uint64_t base_index;
                if (iter_num == 0){
                    base_index = 0;
                }else {
                    base_index = write_to_data_index[cur_write_num];
                }

                if (cur_chunk_num < RING_NUMS){
                    result->flag = -1;
                    return ;
                }
                else {
                    chunk_id = (RING_NUMS - 1) - cur_chunk_num % RING_NUMS;
                }

                if (cur_trans_num != chosen_trans_num){
                    result->index = base_index + cur_trans_num * (8192 * RING_NUMS) + (chunk_id) * 8192 + cur_pac_num * (MTU / 8);
                }
                else{
                    result->index = base_index + cur_trans_num * (8192 * RING_NUMS) + (chunk_id) * (chosen_modal_chunk_size / 8) + cur_pac_num * (MTU / 8);
                }
                
                if (result->index >= DATA_LENGTH){
                    printf("==========WARN : Write index overflow at op 9=========\n");
                }

                // #ifdef DEBUG
                //     if (iter_num == 2){
                //         printf("op 9 cur write num %d cur trans num %d, cur chunk id %d , cur base index %" PRIu64 " cur result index %" PRIu64 " \n", cur_write_num, cur_trans_num, chunk_id, base_index, result->index);
                //         fflush(stdout);
                //     }
                // #endif

			}
			return ;
			break;
		case 7:
			cur_pPr = 74;
			if(bc_gather_num != BC_LENGTH){
				bc_gather_num += len - cur_pPr - 4;
				result->flag = -1;
				#ifdef DEBUG
					printf("==============BROAD PACKET===========\n");
				#endif
			}else{
				result->flag = cur_pPr;

                // index 计算
                cur_pac_num += 1;

                uint64_t base_index;
                if (iter_num == 0){
                    base_index = 0;
                }else {
                    base_index = write_to_data_index[cur_write_num];
                }

                if (cur_chunk_num< RING_NUMS){
                    result->flag = -1;
                    return ;
                }
                else {
                    chunk_id = (RING_NUMS - 1) - cur_chunk_num % RING_NUMS;
                }

                if (cur_trans_num != chosen_trans_num){
                    result->index = base_index + cur_trans_num * (8192 * RING_NUMS) + (chunk_id) * 8192 + cur_pac_num * (MTU / 8);
                }
                else{
                    result->index = base_index + cur_trans_num * (8192 * RING_NUMS) + (chunk_id) * (chosen_modal_chunk_size / 8) + cur_pac_num * (MTU / 8);
                }

                if (result->index >= DATA_LENGTH){
                    printf("==========WARN : Write index overflow at op 7=========\n");
                }

                // #ifdef DEBUG
                //     if (iter_num == 2){
                //         printf("op 7 cur write num %d cur trans num %d, cur chunk id %d , cur base index %" PRIu64 " cur result index %" PRIu64 " \n", cur_write_num, cur_trans_num, chunk_id, base_index, result->index);
                //         fflush(stdout);
                //     }
                // #endif

			}
			return ;
			break;
		case 6:
			cur_pPr = 90;
			if(bc_gather_num != BC_LENGTH){
				bc_gather_num += len - cur_pPr - 4;
				result->flag = -1;
				#ifdef DEBUG
					printf("==============BROAD PACKET===========\n");
				#endif
			}else{
				result->flag = cur_pPr;

                // index 计算
                #ifdef DEBUG
                    // dma_len = bswap_32(pkt_h->dma_length);
                    // printf("cur dma len: %d \n", dma_len);
                #endif

                if (cur_chunk_num == 2 * (RING_NUMS - 1)){
                    cur_trans_num += 1;
                    cur_chunk_num = 0;
                    if (cur_trans_num == chosen_trans_num + 1){
                        // 进入新的iteration
                        cur_trans_num = 0;
                        // iter_num += 1;
                        // #ifdef DEBUG
                        //     printf("====New iteration start===\n");
                        // #endif
                        // 更新chosen变量
                        if (iter_num == 0){
                            // 第一个iteration结束
                            // 第二个iteration： 修改BC， 启用write num
                            // cur_bc_length = BC_LENGTH2;
                            iter_num += 1;
                            #ifdef DEBUG
                                printf("====New iteration start (second iteration)===\n");
                            #endif
                            chosen_trans_num = trans_num1;
                            chosen_last_trans_size = last_trans_size1;
                            chosen_modal_chunk_size = modal_chunk_size1;
                            chosen_last_chunk_size = last_chunk_size1;


                            // 在 new iteration时启动计时组件
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
                        else{
                            // 判断write num来确定iteration 是否发生变动
                            cur_trans_num = 0;
                            cur_bc_length = 0;

                            cur_write_num += 1;
                            if (cur_write_num == RDMA_nums){
                                iter_num += 1;
                                cur_write_num = 0;
                                #ifdef DEBUG
                                    printf("====New iteration start: %d===\n", iter_num);
                                #endif
                                chosen_trans_num = trans_num1;
                                chosen_last_trans_size = last_trans_size1;
                                chosen_modal_chunk_size = modal_chunk_size1;
                                chosen_last_chunk_size = last_chunk_size1;

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
                            else if (cur_write_num % 3 == 1){
                                #ifdef DEBUG
                                    printf("cur write num 1 done. cur write num : %d\n", cur_write_num);
                                #endif
                                chosen_trans_num = trans_num2;
                                chosen_last_trans_size = last_trans_size2;
                                chosen_modal_chunk_size = modal_chunk_size2;
                                chosen_last_chunk_size = last_chunk_size2;
                            }
                            else if (cur_write_num % 3 == 2){
                                #ifdef DEBUG
                                    printf("cur write num 2 done. cur write num : %d\n", cur_write_num);
                                #endif
                                chosen_trans_num = trans_num3;
                                chosen_last_trans_size = last_trans_size3;
                                chosen_modal_chunk_size = modal_chunk_size3;
                                chosen_last_chunk_size = last_chunk_size3;
                            }
                            else if (cur_write_num == RDMA_nums - 1){
                                #ifdef DEBUG
                                    printf("cur write done. Now begin the last RDMA write!\n");
                                #endif
                                chosen_trans_num = trans_num4;
                                chosen_last_trans_size = last_trans_size4;
                                chosen_modal_chunk_size = modal_chunk_size4;
                                chosen_last_chunk_size = last_chunk_size4;
                            }
                            else if (cur_write_num % 3 == 0){
                                #ifdef DEBUG
                                    printf("cur write num 3 done. cur write num : %d\n", cur_write_num);
                                #endif
                                chosen_trans_num = trans_num1;
                                chosen_last_trans_size = last_trans_size1;
                                chosen_modal_chunk_size = modal_chunk_size1;
                                chosen_last_chunk_size = last_chunk_size1;
                            }
                        }
                    }
                }

                cur_pac_num = 0;
                cur_chunk_num += 1;

                #ifdef DEBUG
                    // int cur_RDMA_length;
                    // if (cur_trans_num != trans_num){
                    //     // 非最后一次trans
                    //     cur_RDMA_length = 65536;
                    // }
                    // else if (cur_chunk_num == 1 || cur_chunk_num == RING_NUMS + 1){
                    //     // 最后一次trans的第一次传输 和 第k+1次传输
                    //     cur_RDMA_length = last_chunk_size;
                    // }
                    // else{
                    //     cur_RDMA_length = modal_chunk_size;
                    // }
                    // if (cur_RDMA_length != dma_len){
                    //     printf("cur RDMA length: %d\n", cur_RDMA_length);
                    //     printf("===========WARN: Not Fetch Size==================\n");
                    // }
                #endif

                uint64_t base_index;
                if (iter_num == 0){
                    base_index = 0;
                }else {
                    base_index = write_to_data_index[cur_write_num];
                }

                if (cur_chunk_num < RING_NUMS){
                    result->flag = -1;
                    return ;
                }
                else {
                    chunk_id = (RING_NUMS - 1) - cur_chunk_num % RING_NUMS;
                }

                #ifdef DEBUG
                    // printf("cur chunk num%d, cur chunk id %d\n", cur_chunk_num, chunk_id);
                #endif

                if (cur_trans_num != chosen_trans_num){
                    result->index = base_index + cur_trans_num * (8192 * RING_NUMS) + (chunk_id) * 8192;
                }
                else{
                    result->index = base_index + cur_trans_num * (8192 * RING_NUMS) + (chunk_id) * (chosen_modal_chunk_size / 8);
                }

                if (result->index >= DATA_LENGTH){
                    printf("==========WARN : Write index overflow at op 6=========\n");
                }

                // #ifdef DEBUG
                //     if (iter_num == 2){
                //         printf("op 6 cur write num %d cur trans num %d, cur chunk id %d , cur base index %" PRIu64 " cur result index %" PRIu64 " \n", cur_write_num, cur_trans_num, chunk_id, base_index, result->index);
                //         fflush(stdout);
                //     }
                // #endif

			}
			return ;
			break;
		default: // < 6 and > 9
            result->flag = -1;
			break;
		}

		return ;

    }

    // 检查数据是否全0
    // bool is_all_zero(const char* ptr, size_t len) {
    //     for (size_t i = 0; i < len; ++i) {
    //         if (ptr[i] != 0) {
    //             return false;
    //         }
    //     }
    //     return true;
    // }

    bool is_current_round(const char* ptr){
        uint8_t *round_flag = (uint8_t*)(ptr + 21);
        if ((*round_flag) != round_num){
            #ifdef DEBUG
                // printf("round tag wrong, cur round %d, except flag %d\n", (*round_flag), round_num);
            #endif
            return false;
        }
        return true;
    }

    void run(){
        // 获取共享内存id
        int shm_id;
        // int lcore_nums, i;

        shm_id = shmget(shm_key, SHM_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
        if (shm_id < 0){
            perror("shmget");
            exit(1);
        }

        char *shm_p = (char *)shmat(shm_id, NULL, 0);
        if((void *)shm_p == (void *)-1){
            perror("shmat()");
            exit(1);
        }

        // debug: 运行时清零read index
        // uint64_t *read_index = (uint64_t*)(shm_p + 8);
        // *read_index = 0;

        // uint64_t my_count = 0;

        // 读取共享内存中的 read / write index 来判断是否有新数据
        while (true){
            uint64_t *read_index = (uint64_t*)(shm_p + 16);
            uint64_t *write_index = (uint64_t*)(shm_p + 8);
            #ifdef DEBUG
                // printf("cur read ind: %lld cur write ind: %lld\n", (*read_index), (*write_index));
            #endif

            if ((*write_index) != (*read_index)){
                // 仍然有未解包的数据
                // uint16_t *pac_len = (uint16_t*)(shm_p + 2048 + (*read_index * 2048));
                #ifdef DEBUG
                    // printf("cur pac len: %d\n", (*pac_len));
                #endif

                //读取数据到uint8_t 数组
                // memcpy(dest, shm_p + 2048 + (*read_index * 2048) + 2, (size_t)(*pac_len));
                // while (is_all_zero(shm_p + 2048 + (*read_index * 2048) + 2, 10)){
                //     struct timespec ts;
                //     double sleepTime = 0.0005;
                //     ts.tv_sec = (time_t)sleepTime;
                //     ts.tv_nsec = (long)((sleepTime - ts.tv_sec) * 1e9);
                //     nanosleep(&ts, NULL);
                // }

                while (!is_current_round(shm_p + 2048 + (*read_index * 2048) + 2)){
                    struct timespec ts;
                    double sleepTime = 0.0005;
                    ts.tv_sec = (time_t)sleepTime;
                    ts.tv_nsec = (long)((sleepTime - ts.tv_sec) * 1e9);
                    nanosleep(&ts, NULL);
                }

                uint16_t *pac_len = (uint16_t*)(shm_p + 2048 + (*read_index * 2048));
                memcpy(dest, shm_p + 2048 + (*read_index * 2048) + 2, (size_t)(*pac_len));

                // 调用解包函数进行处理
                decode_hex(dest, (*pac_len), &result);

                // 查看解包结果
                if (result.flag != -1){
                    // 写回解包index 数据
                    uint64_t *payload_index = (uint64_t*)(shm_p + 2048 + (*read_index * 2048) + 2032);
                    uint64_t *res_index = (uint64_t*)(shm_p + 2048 + (*read_index * 2048) + 2040);
                    #ifdef DEBUG
                        // printf("payload index: %d, res_index: %d\n", (result.flag), (result.index));
                        // fflush(stdout);
                    #endif

                    // delay 更新
                    // struct timespec ts;
                    // double sleepTime = 0.00000005;
                    // ts.tv_sec = (time_t)sleepTime;
                    // ts.tv_nsec = (long)((sleepTime - ts.tv_sec) * 1e9);
                    // nanosleep(&ts, NULL);

                    // my_count += 1;
                    // if (my_count % 8000 == 0){
                    //     struct timespec ts;
                    //     double sleepTime = 0.000005;
                    //     ts.tv_sec = (time_t)sleepTime;
                    //     ts.tv_nsec = (long)((sleepTime - ts.tv_sec) * 1e9);
                    //     nanosleep(&ts, NULL);
                    // }

                    *payload_index = result.flag;
                    *res_index = result.index;
                }
                else {
                    uint64_t *payload_index = (uint64_t*)(shm_p + 2048 + (*read_index * 2048) + 2032);
                    uint64_t *res_index = (uint64_t*)(shm_p + 2048 + (*read_index * 2048) + 2040);
                    *payload_index = 0;
                    *res_index = 0;
                }

                if((*read_index +1) >= BUFFER_SIZE){
                    round_num++;
                }
                *read_index = (*read_index + 1) % BUFFER_SIZE;
            }
            else{
                struct timespec ts;
                double sleepTime = 0.001;
                // 将浮点数时间转换为秒和纳秒
                ts.tv_sec = (time_t)sleepTime;
                ts.tv_nsec = (long)((sleepTime - ts.tv_sec)); // 从秒转换为纳秒

                // 调用 nanosleep 休眠
                nanosleep(&ts, NULL);
                // printf("=====CURRENT SLEEPING=====\n");
                // printf("cur read ind: %lld cur write ind: %lld\n", (*read_index), (*write_index));
            }
        }

    }
};



#endif