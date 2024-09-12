## Packet Dumper

We offer two different types of Dump modules for selection, with the default option being assisted dump through the DMA engine. If your environment does not support I/OAT, you can manually compile the Dump module that uses only DPDK cores. The specific compilation instructions are as follows:

```
cd ./dpdk_dump/
make
```

The corresponding start command:

```
./dpdk_dump/l3fwd -l 21 -n 1 -a 0000:6b:00.0 --huge-unlink --file-prefix pg2 -- -P -E -p 0x1 --config="(0,0,21)" --parse-ptype --hash-entry-num=0x111
```