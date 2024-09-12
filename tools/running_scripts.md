
### Run dma dump

```
./dmafwd -l 40-41 -n 1 -a 0000:ca:00.0 -a 0000:80:01.0 -a 0000:80:01.1 -a 0000:80:01.2 -a 0000:80:01.3 -a 0000:80:01.4 -a 0000:80:01.5 -a 0000:80:01.6 -a 0000:80:01.7 --huge-unlink --file-prefix pg1 -- -p 0x1 -c hw -q 8
./dmafwd -l 22-23 -n 1 -a 0000:6b:00.0 -a 0000:00:01.0 -a 0000:00:01.1 -a 0000:00:01.2 -a 0000:00:01.3 -a 0000:00:01.4 -a 0000:00:01.5 -a 0000:00:01.6 -a 0000:00:01.7 --huge-unlink --file-prefix pg2 -- -p 0x1 -c hw -q 8
```

重启后宿主机需要开启GPU direct RDMA:  modprobe nvidia_peermem

登陆交换机查看mac地址链接状态，更新文档的交换机连接配对情况
dis mac-address
重新配置交换机mirror规则;


dpdk 绑定dma engine
echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode


cx6网卡关闭roce
mlxconfig -d 82:00.0 s ROCE_CONTROL=1

