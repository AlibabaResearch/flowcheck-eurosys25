import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.nn.parallel import DistributedDataParallel as DDP
from torch.utils.data import DataLoader, Dataset
from torch.utils.data.distributed import DistributedSampler
import time
from sniffer_v3_P2P import *
import multiprocessing


# 连接共享内存
SHM_KEY = 0x9876
SHM_SIZE = 1*1024*1024*1024
MTU=1024

shm = SharedMemory(SHM_KEY)

CHUNK_SIZE = 2 * MTU
INDEX_SIZE = 2 * MTU

TOTAL_LENGTH = 1000000

class SimpleNet(nn.Module):
    def __init__(self):
        super(SimpleNet, self).__init__()
        self.fc = nn.Linear(1000000000, 1, bias = False)

    def forward(self, x):
        return self.fc(x)


def updater_single():
    # print("enter updater")
    model = SimpleNet()

    # 定义损失函数和优化器
    loss_fn = nn.MSELoss()
    optimizer = optim.SGD(model.parameters(), lr=0.001)
    # 初始化模型gradient
    model.fc.weight.grad = torch.zeros_like(model.fc.weight)
    # 从 pac_processer拿取gradient 并进行checkpoint更新
    copy_time = 0
    update_time = 0
    print("初始化完成")
    start_time = time.time()
    iter_num = 0
    while True:
        print("等待监听")
        # get_gradient = torch.tensor(pac_processer.get_gradient())
        # get_gradient = torch.tensor(input_queue.get())
        # get_gradient = torch.tensor([1000000000])

        get_gradient = None
        # 尝试从共享内存中获取拼接完毕的所有数据
        write_index = shm.read(8, 0)
        write_index = int.from_bytes(write_index, "little")
        if write_index == TOTAL_LENGTH:
            # get_gradient = 
            # TODO : 根据头指针转化成tensor对象 （or 直接用C/Cython写）
        

        if get_gradient is not None:
            # optimizer.zero_grad()
            copy_start = time.time()
            model.fc.weight.grad.view(-1).copy_(get_gradient)
            copy_end = time.time()
            optimizer.step()
            copy_time += copy_end - copy_start
            opt_time = time.time()
            update_time += opt_time - copy_end
            print("监听端 Gradient 已更新！当前iteration:", iter_num)
            iter_num += 1
            if iter_num == 2000:
                end_time = time.time()
                print("监听结束，耗时：", end_time - start_time)
                print("copy用时：", copy_time)
                print("update用时：", update_time)
                print("解包用时:", end_time - start_time - copy_time - update_time)


# if __name__ == "__main__":
# # def mainrun():
#     # seed
#     torch.manual_seed(2024)
#     torch.cuda.manual_seed(2024)

#     queue = multiprocessing.Queue()

#     # 开始监听
#     # CAPTURE_NODE_ADDR = '0c:42:a1:9f:58:f8'  # 监听节点MAC地址 rank0
#     CAPTURE_NODE_ADDR = 0x0c42a19f58f9
#     RING_NUMS = 2
#     MTU = 1024
#     pac_processer = PackageProcesser(MTU, 1000000, CAPTURE_NODE_ADDR, RING_NUMS)

#     # 创建一个进程用来监听+解包
#     process_a = multiprocessing.Process(target = pac_processer.run, args = (queue,))
#     # 创建一个进程用来update
#     process_b = multiprocessing.Process(target = updater, args = (queue,))
    
#     process_a.start()
#     process_b.start()

#     process_a.join()
#     process_b.join()

if __name__ == "__main__":
#     import cProfile
#     cProfile.run('mainrun()')
    updater_single()