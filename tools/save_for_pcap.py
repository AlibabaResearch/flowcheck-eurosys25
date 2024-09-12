import scapy

from scapy.all import *
from scapy.all import Packet,BitField,BitEnumField,XBitField,IntField,IPField,IP,UDP
from sysv_ipc import SharedMemory, Semaphore


# SHM_KEY = 0x5678
SHM_KEY = 0x3456
SHM_SIZE = 25*1024*1024*1024
MTU=1024

shm = SharedMemory(SHM_KEY)

CHUNK_SIZE = 2 * MTU
INDEX_SIZE = 2 * MTU

if __name__ == "__main__":
    offset = 0
    pac_list = []
    test_num = 0

    read_index = 0

    while True:
        # index_flag
        # read_index = shm.read(8, 16)
        # read_index = int.from_bytes(read_index, "little")
        write_index = shm.read(8, 8)
        write_index = int.from_bytes(write_index, "little")

        # print(read_index)
        # print(write_index)
        
        if(write_index == read_index):
            print("No packet need to process")
            # continue 
            break

        length = int.from_bytes(shm.read(2, offset+INDEX_SIZE), "little")
        # print("length:",length)

        # 创建一个字符串缓冲区，大小与共享内存大小匹配
        buffer = shm.read(length, offset+INDEX_SIZE + 2)
        # 打印共享内存中的内容
        # print(buffer)
        # print(type(buffer))
        # print(trimmed_data)
        
        # 将其传递给核心解包逻辑函数
        # self.transition(trimmed_data)
        # self.transition(buffer)

        # 进行数据包保存
        try:
            packet = Ether(buffer)
            pac_list.append(packet)
        except:
            print("WARN")
            pass

        # 获取解包结果 
        pay_index = shm.read(8, 2048 + 2048 * read_index + 2032)
        pay_index = int.from_bytes(pay_index, "little")

        data_index = shm.read(8, 2048 + 2048 * read_index + 2040)
        data_index = int.from_bytes(data_index, "little")

        print("cur pay_index:", pay_index, "cur data_index:", data_index, "pac_id:", read_index)



        offset += CHUNK_SIZE

        # 更改read index
        read_index += 1
        # shm.write(read_index.to_bytes(8, 'little'), 16)

        test_num += 1
        if test_num == 5000:
            break

    # 保存成文件
    # output_file = 'output-20M-dma.pcapng'
    # output_file = 'output-1M-outbound-1QP.pcapng'
    # output_file = 'output-single-decoder-full-0122-1QP.pcapng'
    output_file = 'output-test3.pcapng'
    wrpcapng(output_file, pac_list)
    print(f"Packet saved to {output_file}")
    print("Saved" + str(len(pac_list)) + "pacs.")

    # sem.release()
    shm.detach()
