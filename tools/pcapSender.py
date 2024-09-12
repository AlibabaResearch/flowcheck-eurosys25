from scapy.all import *
from scapy.utils import rdpcap
import sys

fName,iface = sys.argv[1],sys.argv[2]
pkts=rdpcap(fName) 
for pkt in pkts:
     sendp(pkts,iface=iface,loop=0,inter=0.02) #sending packet at layer 
     
# from scapy.all import *
# import sys
# mode,dip,vni,pkt_count = sys.argv[1],sys.argv[2],sys.argv[3],int(sys.argv[4])
# print(pkt_count)
# print(mode,dip,vni,pkt_count)
# if sys.argv[1]=='u':
#     pad = 'x'*100
#     base_pkt = Ether()/IP(src="192.168.60.92",dst=dip)/UDP(dport=4789,sport=10000)/VXLAN(vni=int(vni))/Ether()/IP(src="2.2.2.1",dst=dip)/UDP(dport=1000,sport=2000)/ pad
#     sendp(base_pkt,iface='ens9f0',loop=0,inter=0.02,count=pkt_count)
#     base_pkt.show()
# else:
#     pass