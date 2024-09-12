export NCCL_DEBUG=INFO
export NCCL_DEBUG_SUBSYS=ALL
#export NCCL_SOCKET_IFNAME=eth0
export NCCL_IB_HCA=mlx5_0
export NCCL_IB_GID_INDEX=1
export NCCL_MIN_NCHANNELS=1
export NCCL_MAX_NCHANNELS=1
export NCCL_CROSS_NIC=2
export NCCL_NET_GDR_LEVEL=1
export NCCL_IBEXT_DISABLE=1
export NCCL_ALGO=Ring
export NCCL_ASYNC_ERROR_HANDLING=1
export NCCL_PROTO=LL
export NCCL_IB_SPLIT_DATA_ON_QPS=0
export NCCL_P2P_DISABLE=1
export NCCL_SHM_DISABLE=1
# export TORCH_DISTRIBUTED_DEBUG=DETAIL
# python trans1.py
python -m torch.distributed.launch --nproc_per_node=1 --nnodes=8 --node_rank=0 --master_addr="33.33.33.80" --master_port=12012 gpt_test.py
