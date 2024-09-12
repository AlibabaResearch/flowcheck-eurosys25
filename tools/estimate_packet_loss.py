# [Temp Tool Script] 估算不同丢包处理逻辑下的丢包率
import math
# --------------------------------------------------------------
# 使用参数定义
# --------------------------------------------------------------
p = 1e-8 # 一跳丢包率
Ring_Size = 24
Group_size = 32
Mirror_Size = 10 # Mirror流中的总数据量，单位为GB
MTU = 1024 

# --------------------------------------------------------------
# 预处理
# --------------------------------------------------------------
Mirror_Pac_Nums = int(Mirror_Size * 1024 * 1024 * 1024 / MTU)

def SingleMirrorLoss():
    # 增添通信步，mirror一份单方向的流，总共得到k个数据块，期望恢复k个数据块
    return (1-p) ** Mirror_Pac_Nums

def TwoFullMirrorLoss():
    # 增添通信步，mirror两份同一个单方向的流，总共得到2k个数据块，期望恢复k个数据块
    return (1-p ** 2) ** Mirror_Pac_Nums

def TwoPartMirrorLoss():
    # 不增添通信步，mirror两份不同单方向的流，总共得到2(k-1)个数据块，期望恢复k个数据块
    double_backup_pacs = Mirror_Pac_Nums * (Ring_Size - 2) / Ring_Size
    no_backup_pacs = Mirror_Pac_Nums * (2) / Ring_Size
    return (1-p ** 2) ** double_backup_pacs * (1-p) ** no_backup_pacs
    

def TwoStageLoss():
    # TODO： 存在 BUG ，单次重传成功概率不是单块不丢包率！
    # 不增添通信步，Switch Mirror分为两个阶段，第一阶段用两个mirror流获取完整数据，第二阶段根据上一阶段需要重传的包进行重新mirror
    single_chunk_size = Mirror_Pac_Nums / Ring_Size
    p_single_chunk = (1-p) ** single_chunk_size # 单块不丢包率
    print("[DEBUG]单块不丢包率:", p_single_chunk)
    # p_all_single_not_loss = p_single_chunk ** Ring_Size # 所有单块不丢包
    # print("[DEBUG]所有单块不丢包:", p_all_single_not_loss)
    p_loss_chunk_larger_than_stage2_step = 0 # 丢包超过k/2块
    for m in range(int(Ring_Size/2),Ring_Size + 1):
        p_loss_chunk_larger_than_stage2_step += math.comb(Ring_Size, m) * p_single_chunk ** (Ring_Size - m) * (1-p_single_chunk) ** m
    print("[DEBUG]丢包超过k/2块:", p_loss_chunk_larger_than_stage2_step)
    p_resend_success = 0 
    for m in range(0,int(Ring_Size/2)):
        p_loss_m = math.comb(Ring_Size, m) * p_single_chunk ** (Ring_Size - m) * (1-p_single_chunk) ** m # 丢包m块概率
        # print(" -- [DEBUG] 丢包" + str(m)  + "块概率:", p_loss_m)
        cur_p = 0
        for i in range(0,m):
            cur_p += math.comb(int(Ring_Size/2)-1, i) * p_single_chunk ** i * (1-p_single_chunk) ** (Ring_Size/2-1 - i) # 在 k/2-1步 中 重传成功少于m次的概率
        cur_resend_success= 1 - cur_p  # 在 k/2-1步 中 重传成功大于等于m次的概率
        p_resend_success += cur_resend_success * p_loss_m
    # p_resend_fail = 1 - p_loss_chunk_larger_than_stage2_step - p_resend_success
    print("[DEBUG]丢包小于k/2,重传成功率:", p_resend_success)
    # return 1 - p_loss_chunk_larger_than_stage2_step - p_resend_fail
    return p_resend_success


def systemLossRate(group_loss_rate):
    # 只要有一组有问题，整个系统都会受到一定影响
    return group_loss_rate ** Group_size


if __name__ == "__main__":
    # 单个监听组情况
    print("Mirror单方向流（额外步），无冗余，单个组不丢包率：",SingleMirrorLoss())
    print("Mirror单方向流（额外步），两个流冗余，单个组不丢包率：",TwoFullMirrorLoss())
    print("Mirror双方向流（无额外步），两个流冗余，单个组不丢包率：",TwoPartMirrorLoss())
    print("交换机Stage方案，单个组不丢包率：",TwoStageLoss())

    # 整个系统丢包率
    print("-" * 80)
    print("Mirror单方向流（额外步），无冗余，系统不丢包率：",systemLossRate(SingleMirrorLoss()))
    print("Mirror单方向流（额外步），两个流冗余，系统不丢包率：",systemLossRate(TwoFullMirrorLoss()))
    print("Mirror双方向流（无额外步），两个流冗余，系统不丢包率：",systemLossRate(TwoPartMirrorLoss()))
    print("交换机Stage方案，系统不丢包率：",systemLossRate(TwoStageLoss()))