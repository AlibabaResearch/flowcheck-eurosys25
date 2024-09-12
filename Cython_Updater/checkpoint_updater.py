from mem_extract_c import PyExtractor
import numpy as numpy
import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.nn.parallel import DistributedDataParallel as DDP
from torch.utils.data import DataLoader, Dataset
from torch.utils.data.distributed import DistributedSampler
import numpy as np
import time
from sysv_ipc import SharedMemory, Semaphore

torch.set_num_threads(4)

TOTAL_LENGTH = 1000000

class SimpleNet(nn.Module):
    def __init__(self):
        super(SimpleNet, self).__init__()
        self.fc = nn.Linear(TOTAL_LENGTH, 1, bias = False,dtype=torch.float32)
        self.loss_fn = nn.MSELoss()
        self.optimizer = optim.SGD(self.parameters(), lr=0.001)
        # 初始化模型gradient
        self.fc.weight.grad = torch.zeros_like(self.fc.weight)
        # 从 pac_processer拿取gradient 并进行checkpoint更新
        

    def forward(self, x):
        return self.fc(x)

# debug
def manual_get_data():
    SHM_KEY = 0x9876
    SHM_SIZE = 1*1024*1024*1024
    MTU=1024

    shm = SharedMemory(SHM_KEY)
    # first_bytes = int.from_bytes(shm.read(4, 2048), "little")
    # print("first bytes:", first_bytes)
    first_read = shm.read(1024,2048)
    print("first read:", first_read)


def updater():
    model = SimpleNet()
    loss_fn = nn.MSELoss()
    optimizer = optim.SGD(model.parameters(), lr=0.001)
    py_extractor = PyExtractor()
    
    print("model init done.")
    iter_num = 0

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
                get_gradient.resize_(model.fc.weight.size())
                if get_gradient is not None:
                    # print("等待监听")
                    print(get_gradient)
                    model.fc.weight.grad = get_gradient
                    # copy_end = time.time()
                    model.optimizer.step()
                    iter_num += 1



if __name__ == "__main__":
    updater()