# stage 1

from torch.nn import functional as F
from transformers import GPT2Config, GPT2LMHeadModel
import os
import torch
import torch.distributed as dist
import torch.nn as nn
import torch.optim as optim
from torch.nn.parallel import DistributedDataParallel as DDP
from torch.utils.data import DataLoader, Dataset, TensorDataset
from torch.utils.data.distributed import DistributedSampler
import time


class TempLLama(nn.Module):
    def __init__(self, vocab_size, num_layers, num_heads, model_dim, max_seq_length):
        super().__init__()
        self.embed_tokens = nn.Embedding(vocab_size, model_dim)
        self.positional_encoding = nn.Parameter(torch.zeros(max_seq_length, model_dim))
        self.layers = nn.ModuleList([nn.TransformerEncoderLayer(
            d_model=model_dim, 
            nhead=num_heads, 
            dim_feedforward=model_dim * 4,  # Just an example size
            dropout=0.1
        ) for _ in range(num_layers)])
        self.output_layer = nn.Linear(model_dim, vocab_size, bias=False)

    def forward(self, input_ids):
        seq_length = input_ids.size(1)
        token_embeddings = self.embed_tokens(input_ids) * (self.model_dim ** 0.5)  # Scale embeddings
        positional_encodings = self.positional_encoding[:seq_length, :]
        x = token_embeddings + positional_encodings
        x = F.dropout(x, p=0.1, training=self.training)

        for layer in self.layers:
            x = layer(x)

        logits = self.output_layer(x)
        return logits


def setup(rank, world_size):
    os.environ['MASTER_ADDR'] = '33.33.33.80'
    os.environ['MASTER_PORT'] = '12012'
    
    # 初始化进程组
    dist.init_process_group("nccl", rank=rank, world_size=world_size)

def cleanup():
    dist.destroy_process_group()

def create_random_data(batch_size, sequence_length, vocab_size):
    return torch.randint(0, vocab_size, (batch_size, sequence_length))


def get_model_seg(seg_id):

    # 配置模型参数来匹配GPT-3 1.3B的规模
    # config = GPT2Config(
    #     vocab_size=50257,  # GPT-3的词汇量
    #     n_positions=1024,  # 序列的最大长度
    #     n_ctx=1024,        # 上下文长度
    #     n_embd=2048,       # 嵌入层的大小
    #     n_layer=24,        # Transformer层的数量
    #     n_head=16,         # 注意力头的数量
    #     attn_pdrop=0.1,    # 注意力层的dropout概率
    #     resid_pdrop=0.1,   # 残差连接的dropout概率
    #     embd_pdrop=0.1     # 嵌入层的dropout概率
    # )

    config = GPT2Config(
        vocab_size=50257,
        n_positions=1024,
        n_ctx=1024,
        n_embd=768,
        n_layer=12,
        n_head=12,
        attn_pdrop=0.1,
        resid_pdrop=0.1,
        embd_pdrop=0.1
    )

    # config = GPT2Config(
    #     vocab_size=50257,
    #     n_positions=1024,
    #     n_ctx=1024,
    #     n_embd=1024,
    #     n_layer=24,
    #     n_head=16,
    #     attn_pdrop=0.1,
    #     resid_pdrop=0.1,
    #     embd_pdrop=0.1
    # )

    full_model = GPT2LMHeadModel(config)
    # 将模型分成两部分

    if seg_id == 0:
        model_seg = nn.Sequential(
            full_model.transformer.wte,  # Token embedding
            full_model.transformer.wpe,  # Positional embedding
            *full_model.transformer.h[:n_layer_each]  # 前n_layer_each层
        )
    elif seg_id == 1:
        model_seg = nn.Sequential(
            *full_model.transformer.h[-n_layer_each:],  # 后n_layer_each层
            # full_model.lm_head  # LM头部，负责将hidden states转换为vocab size logits
            full_model.ln_f
        )
    return model_seg

# 构建DDP训练
def main(rank, world_size, gpu_id):
    setup(rank, world_size)

    # 配置模型参数来匹配GPT-3 1.3B的规模
    # config = GPT2Config(
    #     vocab_size=50257,  # GPT-3的词汇量
    #     n_positions=1024,  # 序列的最大长度
    #     n_ctx=1024,        # 上下文长度
    #     n_embd=2048,       # 嵌入层的大小
    #     n_layer=24,        # Transformer层的数量
    #     n_head=16,         # 注意力头的数量
    #     attn_pdrop=0.1,    # 注意力层的dropout概率
    #     resid_pdrop=0.1,   # 残差连接的dropout概率
    #     embd_pdrop=0.1     # 嵌入层的dropout概率
    # )

    # config = GPT2Config(
    #     vocab_size=50257,
    #     n_positions=1024,
    #     n_ctx=1024,
    #     n_embd=768,
    #     n_layer=12,
    #     n_head=12,
    #     attn_pdrop=0.1,
    #     resid_pdrop=0.1,
    #     embd_pdrop=0.1
    # )

    # config = GPT2Config(
    #     vocab_size=50257,
    #     n_positions=1024,
    #     n_ctx=1024,
    #     n_embd=1024,
    #     n_layer=24,
    #     n_head=16,
    #     attn_pdrop=0.1,
    #     resid_pdrop=0.1,
    #     embd_pdrop=0.1
    # )

    config = GPT2Config(
        vocab_size=50257,
        n_positions=1024,
        n_ctx=1024,
        n_embd=4096,
        n_layer=40,
        n_head=32,
        attn_pdrop=0.1,
        resid_pdrop=0.1,
        embd_pdrop=0.1
    )

    # config = GPT2Config(
    #     vocab_size=50257,
    #     n_positions=1024,
    #     n_ctx=1024,
    #     n_embd=2048,
    #     n_layer=32,
    #     n_head=16,
    #     attn_pdrop=0.1,
    #     resid_pdrop=0.1,
    #     embd_pdrop=0.1
    # )

    # 根据配置实例化模型
    model = GPT2LMHeadModel(config)

    # 把模型放到GPU上（如果可用）
    model.to(gpu_id)
    ddp_model = DDP(model, device_ids=[gpu_id], broadcast_buffers = False)

    # 输出模型的信息，确保参数数量大约是1.3B
    print("Number of parameters:", model.num_parameters())

    optimizer = optim.Adam(ddp_model.parameters(), lr=0.0001)

    # 所有参数设1
    # for name, param in model.named_parameters():
    #     if param.requires_grad:
    #         # 假设我们想要将所有参数的梯度设置为1
    #         param.grad = torch.ones_like(param)

    # # 统计所有参数
    # total_params = 0
    # for param_name, param in model.named_parameters():
    #     if param.grad is not None:
    #     # if param.requires_grad:
    #         total_params += param.grad.numel()
    #         # print("cur params:", param.grad.numel(), "current total num:", total_params)
    #         print(f"Param {param_name}, Gradient Size {param.grad.shape}, current total param {total_params}")

    # print("total params:", total_params)

    print("begin to train")
    batch_size = 2
    sequence_length = 128

    start_time = time.time()

    for epoch in range(1):
        # DistributedSampler确定每个进程处理数据子集
        
        # transformer_decoder.train()
        inputs = create_random_data(batch_size, sequence_length, config.vocab_size).to(gpu_id)
        labels = create_random_data(batch_size, sequence_length, config.vocab_size).to(gpu_id)
        
        optimizer.zero_grad()
        output = ddp_model(inputs, labels=labels)
        loss = output.loss
        # loss = criterion(output.reshape(-1, vocab_size), tgt_batch.reshape(-1))

        # 手动设置梯度
        tol_p = 1
        # for name, param in ddp_model.named_parameters():
        #     if param.requires_grad:
        #         num_elements = param.numel()
        #         # param.grad = torch.arange(tol_p, num_elements + tol_p, dtype=torch.int32, device=param.device).reshape_as(param)
        #         param.grad = torch.arange(tol_p, num_elements + tol_p, dtype=param.dtype, device=param.device).reshape_as(param)
        #         tol_p += num_elements

        loss.backward()

        optimizer.step()
        
        time.sleep(12)
        # 只测一个iteration
        # break

    torch.save(ddp_model.state_dict(), 'temp.pt')

    end_time = time.time()

    print("total time:", end_time - start_time, "cur_iter_time:", (end_time - start_time) / 2)
    cleanup()


if __name__ == "__main__":
    # seed
    torch.manual_seed(2024)
    torch.cuda.manual_seed(2024)
    torch.cuda.manual_seed_all(2024)

    # 当前进程的GPU设备ID
    rank = 0
    # 确定使用的总GPU数量
    world_size = 8
    gpu_id = 0

    main(rank, world_size, gpu_id)
