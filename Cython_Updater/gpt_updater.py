from mem_extract_c import PyExtractor
import numpy as numpy
import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.nn.parallel import DistributedDataParallel as DDP
from torch.utils.data import DataLoader, Dataset
from torch.utils.data.distributed import DistributedSampler
from transformers import GPT2Config, GPT2LMHeadModel
import numpy as np
import time
from sysv_ipc import SharedMemory, Semaphore

torch.set_num_threads(4)

TOTAL_LENGTH = 1313626112

# debug
# def manual_get_data():
#     SHM_KEY = 0x9876
#     SHM_SIZE = 1*1024*1024*1024
#     MTU=1024

#     shm = SharedMemory(SHM_KEY)
#     # first_bytes = int.from_bytes(shm.read(4, 2048), "little")
#     # print("first bytes:", first_bytes)
#     first_read = shm.read(1024,2048)
#     print("first read:", first_read)


def updater():
    # 配置模型参数来匹配GPT-3 1.3B的规模
    config = GPT2Config(
        vocab_size=50257,  # GPT-3的词汇量
        n_positions=1024,  # 序列的最大长度
        n_ctx=1024,        # 上下文长度
        n_embd=2048,       # 嵌入层的大小
        n_layer=24,        # Transformer层的数量
        n_head=16,         # 注意力头的数量
        attn_pdrop=0.1,    # 注意力层的dropout概率
        resid_pdrop=0.1,   # 残差连接的dropout概率
        embd_pdrop=0.1     # 嵌入层的dropout概率
    )

    # 根据配置实例化模型
    model = GPT2LMHeadModel(config)

    optimizer = optim.Adam(model.parameters(), lr=0.0001)

    py_extractor = PyExtractor()
    
    print("model init done.")
    iter_num = 0

    # return

    # debug
    # manual_get_data()

    while True:
        with torch.no_grad():
            for i in range(1):
                # 从cython接口获取完整gradient
                # t = ctoTensor()

                t = py_extractor.getTensor(TOTAL_LENGTH)
                print("get res from cython")
                get_gradient = torch.from_numpy(np.asarray(t))

                # get_gradient.resize_(model.fc.weight.size())

                if get_gradient is not None:
                    # print("等待监听")
                    # print(get_gradient)

                    pointer = 0
                    for param in model.parameters():
                        num_param = param.numel()
                        grad = get_gradient[pointer:pointer + num_param].view_as(param)
                        param.grad = grad
                        pointer += num_param
                    
                    print("gradient load done.")
                    # model.fc.weight.grad = get_gradient
                    
                    # copy_end = time.time()
                    # model.optimizer.step()
                    start_opt = time.time()
                    optimizer.step()
                    end_opt = time.time()
                    iter_num += 1
                    print("model update done. model update time:", end_opt - start_opt)



if __name__ == "__main__":
    # seed
    torch.manual_seed(2024)
    # torch.cuda.manual_seed(2024)
    # torch.cuda.manual_seed_all(2024)
    updater()