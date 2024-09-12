/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright(c) 2017-2018 Linaro Limited.
 */

#ifndef __L3FWD_EM_HLM_H__
#define __L3FWD_EM_HLM_H__

#if defined RTE_ARCH_X86
#include "l3fwd_sse.h"
#include "l3fwd_em_hlm_sse.h"
#elif defined __ARM_NEON
#include "l3fwd_neon.h"
#include "l3fwd_em_hlm_neon.h"
#endif

#ifdef RTE_ARCH_ARM64
#define EM_HASH_LOOKUP_COUNT 16
#else
#define EM_HASH_LOOKUP_COUNT 8
#endif
#include <rte_ib.h>
#include <rte_udp.h>
#include <rte_spinlock.h>
#include "net/rte_reth.h"
#include "net/rte_immdt.h"
#include "shared.h"
// #include <stdint.h>

// 定义所有ipv6包数便于统计
int TOTAL_IPV6_NUM = 0;
int TOTAL_IPV4_NUM = 0;
int TOTAL_PACS = 0;
int COPY_PAYLOAD_LENGTH = 0;
int WRITE_PAC_NUM = 0;

uint64_t BUFFER_SIZE = (4ULL * 1024 * 1024 * 1024 / 2048) - 1;


static __rte_always_inline void
copy_payload_to_new_memory(struct rte_mbuf *m, uint16_t header_size, char* shm_p) {
    uint16_t packet_length = rte_pktmbuf_pkt_len(m);
    char *payload = rte_pktmbuf_mtod_offset(m, char *, header_size); // Skip the header size

    uint64_t* write_index = (uint64_t*)(shm_p);
    uint64_t* read_index = (uint64_t*)(shm_p + sizeof(uint64_t));
    // rte_spinlock_t* write_lock = (rte_spinlock_t*)(shm_p + 2 * sizeof(uint64_t));


    // rte_spinlock_lock(write_lock);
    uint16_t *buffer_packet_length = (uint16_t *)(shm_p + 2048 + (*write_index * 2048));
    *buffer_packet_length = packet_length;
	*write_index = (*write_index + 1) % BUFFER_SIZE;
	// rte_spinlock_unlock(write_lock);

    rte_memcpy(buffer_packet_length + 1, payload, packet_length); // Copy after the length field
    
    if ((*write_index + 1) % BUFFER_SIZE == *read_index) {
        RTE_LOG(INFO, L3FWD, "Packet buffer overflow!\n");
    }

    WRITE_PAC_NUM++; // Increase packet count, assuming it's a global or static variable
}

static __rte_always_inline void
em_get_dst_port_ipv4xN(struct lcore_conf *qconf, struct rte_mbuf *m[],
		uint16_t portid, uint16_t dst_port[])
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv4_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		get_ipv4_5tuple(m[i], mask0.x, &key[i]);
		key_array[i] = &key[i];
	}

	rte_hash_lookup_bulk(qconf->ipv4_lookup_struct, &key_array[0],
			     EM_HASH_LOOKUP_COUNT, ret);

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		dst_port[i] = ((ret[i] < 0) ?
				portid : ipv4_l3fwd_out_if[ret[i]]);

		if (dst_port[i] >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port[i]) == 0)
			dst_port[i] = portid;
	}
}

static __rte_always_inline void
em_get_dst_port_ipv6xN(struct lcore_conf *qconf, struct rte_mbuf *m[],
		uint16_t portid, uint16_t dst_port[], char *shm_p)
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv6_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	struct rte_ib_bth *ib_hdr;
	struct rte_reth_bth *reth_hdr;
	struct rte_immdt_bth *immdt_hdr;


	for (i=0; i < EM_HASH_LOOKUP_COUNT; i++){
		/*
		size_t skip_to_ib = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv6_hdr) + sizeof(struct rte_udp_hdr);
		ib_hdr = rte_pktmbuf_mtod_offset(m[i], struct rte_ib_bth *, skip_to_ib);
		size_t skip_after_ib = skip_to_ib + sizeof(struct rte_ib_bth);
		// RTE_LOG(INFO, L3FWD, "Ib层读取完成, 当前op: %d, QP: %d  PSN: %d \n", ib_hdr->opcode, qpValue, psnValue);
		
		if (ib_hdr->opcode >= 6 && ib_hdr->opcode <= 9){ // 感兴趣opcode

			// 查看IB层相关信息
			uint32_t psnValue = (ib_hdr->psn[0] << 16) | (ib_hdr->psn[1] << 8) | ib_hdr->psn[2];
			uint32_t qpValue = (ib_hdr->dst_qp[0] << 16) | (ib_hdr->dst_qp[1] << 8) | ib_hdr->dst_qp[2];
			RTE_LOG(INFO, L3FWD, "Ib层读取完成, 当前op: %d, QP: %d  PSN: %d \n", ib_hdr->opcode, qpValue, psnValue);

			// opcode == 6: 查看RETH层相关信息
			if (ib_hdr->opcode == 6){
				reth_hdr = rte_pktmbuf_mtod_offset(m[i], struct rte_reth_bth *, skip_after_ib);
				uint64_t vaddr = (reth_hdr->virtual_address[0] << 56) | (reth_hdr->virtual_address[1] << 48) | (reth_hdr->virtual_address[2] << 40) | 
							(reth_hdr->virtual_address[3] << 32) | (reth_hdr->virtual_address[4] << 24) | (reth_hdr->virtual_address[5] << 16) | 
							(reth_hdr->virtual_address[6] << 8) | (reth_hdr->virtual_address[7]);
				uint32_t dma_length = (reth_hdr->DMA_length[0] << 24) | (reth_hdr->DMA_length[1] << 16) | (reth_hdr->DMA_length[2] << 8) | 
							(reth_hdr->DMA_length[3]);
				// uint64_t vaddr = 
				RTE_LOG(INFO, L3FWD, "RETH层读取完成, 当前虚拟地址: %llu , 当前DMA length: %llu \n", vaddr, dma_length);
				// RTE_LOG(INFO, L3FWD, "RETH层读取完成, 当前虚拟地址: %llu , 当前DMA length: %llu \n", *reinterpret_cast<const int64_t*>(reth_hdr->virtual_address), *reinterpret_cast<const int32_t*>(reth_hdr->DMA_length));
				skip_after_ib += sizeof(struct rte_reth_bth);
			}

			// opcode == 9: 查看IMMDT层相关信息
			if (ib_hdr->opcode == 9){
				immdt_hdr = rte_pktmbuf_mtod_offset(m[i], struct rte_immdt_bth *, skip_after_ib);
				RTE_LOG(INFO, L3FWD, "IMMDT层读取完成\n");
				skip_after_ib += sizeof(struct rte_immdt_bth);
			}

			*/
			// payload + 最后4bytes的 ICRC
			
			// payload copy
			copy_payload_to_new_memory(m[i], 0, shm_p);
		}
}

static __rte_always_inline void
em_get_dst_port_ipv4xN_events(struct lcore_conf *qconf, struct rte_mbuf *m[],
			      uint16_t dst_port[])
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv4_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		get_ipv4_5tuple(m[i], mask0.x, &key[i]);
		key_array[i] = &key[i];
	}

	rte_hash_lookup_bulk(qconf->ipv4_lookup_struct, &key_array[0],
			     EM_HASH_LOOKUP_COUNT, ret);

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		dst_port[i] = ((ret[i] < 0) ?
				m[i]->port : ipv4_l3fwd_out_if[ret[i]]);

		if (dst_port[i] >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port[i]) == 0)
			dst_port[i] = m[i]->port;
	}
}

static __rte_always_inline void
em_get_dst_port_ipv6xN_events(struct lcore_conf *qconf, struct rte_mbuf *m[],
			      uint16_t dst_port[])
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv6_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		get_ipv6_5tuple(m[i], mask1.x, mask2.x, &key[i]);
		key_array[i] = &key[i];
	}

	rte_hash_lookup_bulk(qconf->ipv6_lookup_struct, &key_array[0],
			     EM_HASH_LOOKUP_COUNT, ret);

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		dst_port[i] = ((ret[i] < 0) ?
				m[i]->port : ipv6_l3fwd_out_if[ret[i]]);

		if (dst_port[i] >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port[i]) == 0)
			dst_port[i] = m[i]->port;
	}
}

static __rte_always_inline uint16_t
em_get_dst_port(const struct lcore_conf *qconf, struct rte_mbuf *pkt,
		uint16_t portid, char *shm_p)
{
	uint16_t next_hop;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct rte_ib_bth *ib_hdr;
	struct rte_reth_bth *reth_hdr;
	struct rte_immdt_bth *immdt_hdr;
	uint32_t tcp_or_udp;
	uint32_t l3_ptypes;

	// RTE_LOG(INFO, L3FWD, "Packet Type: 0x%x\n", pkt->packet_type); // 0x210

	tcp_or_udp = pkt->packet_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);
	l3_ptypes = pkt->packet_type & RTE_PTYPE_L3_MASK;

	// RTE_LOG(INFO, L3FWD, "tcp/udp Type: 0x%x\n", tcp_or_udp); // 0x200
	// RTE_LOG(INFO, L3FWD, "l3 Type: 0x%x\n", l3_ptypes); // 0x10

	if (tcp_or_udp && (l3_ptypes == RTE_PTYPE_L3_IPV4)) {
		// RTE_LOG(INFO, L3FWD, "Handing IPV4\n");
		TOTAL_IPV4_NUM += 1;
		/* Handle IPv4 headers.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *,
				sizeof(struct rte_ether_hdr));

		next_hop = em_get_ipv4_dst_port(ipv4_hdr, portid,
				qconf->ipv4_lookup_struct);

		if (next_hop >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << next_hop) == 0)
			next_hop = portid;

		return next_hop;

	} else if (tcp_or_udp && (l3_ptypes == RTE_PTYPE_L3_IPV6)) {
		// RTE_LOG(INFO, L3FWD, "Handing IPV6\n");
		TOTAL_IPV6_NUM += 1;
		/*
		// 替换成对应的结构体解析逻辑
		// TODO: 在处理这个包时，prefetch这个包的payload和下个包的payload

		size_t skip_to_ib = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv6_hdr) + sizeof(struct rte_udp_hdr); // TODO: 这一行替换成常量
		ib_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ib_bth *, skip_to_ib);
		size_t skip_after_ib = skip_to_ib + sizeof(struct rte_ib_bth); // TODO: 这一行替换成常量
		// RTE_LOG(INFO, L3FWD, "Ib层读取完成, 当前op: %d, QP: %d  PSN: %d \n", ib_hdr->opcode, qpValue, psnValue);
		
		if (ib_hdr->opcode >= 6 && ib_hdr->opcode <= 9){ // 感兴趣opcode
			// 查看IB层相关信息
			uint32_t psnValue = (ib_hdr->psn[0] << 16) | (ib_hdr->psn[1] << 8) | ib_hdr->psn[2];
			uint32_t qpValue = (ib_hdr->dst_qp[0] << 16) | (ib_hdr->dst_qp[1] << 8) | ib_hdr->dst_qp[2];
			RTE_LOG(INFO, L3FWD, "Ib层读取完成, 当前op: %d, QP: %d  PSN: %d \n", ib_hdr->opcode, qpValue, psnValue);
			// opcode == 6: 查看RETH层相关信息
			if (ib_hdr->opcode == 6){
				reth_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_reth_bth *, skip_after_ib);
				uint64_t vaddr = (reth_hdr->virtual_address[0] << 56) | (reth_hdr->virtual_address[1] << 48) | (reth_hdr->virtual_address[2] << 40) | 
							(reth_hdr->virtual_address[3] << 32) | (reth_hdr->virtual_address[4] << 24) | (reth_hdr->virtual_address[5] << 16) | 
							(reth_hdr->virtual_address[6] << 8) | (reth_hdr->virtual_address[7]);
				uint32_t dma_length = (reth_hdr->DMA_length[0] << 24) | (reth_hdr->DMA_length[1] << 16) | (reth_hdr->DMA_length[2] << 8) | 
							(reth_hdr->DMA_length[3]);
				// uint64_t vaddr = 
				RTE_LOG(INFO, L3FWD, "RETH层读取完成, 当前虚拟地址: %llu , 当前DMA length: %llu \n", vaddr, dma_length);
				// RTE_LOG(INFO, L3FWD, "RETH层读取完成, 当前虚拟地址: %llu , 当前DMA length: %llu \n", *reinterpret_cast<const int64_t*>(reth_hdr->virtual_address), *reinterpret_cast<const int32_t*>(reth_hdr->DMA_length));
				skip_after_ib += sizeof(struct rte_reth_bth);
			}

			// opcode == 9: 查看IMMDT层相关信息
			if (ib_hdr->opcode == 9){
				immdt_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_immdt_bth *, skip_after_ib);
				RTE_LOG(INFO, L3FWD, "IMMDT层读取完成\n");
				skip_after_ib += sizeof(struct rte_immdt_bth);
			}

			// payload + 最后4bytes的 ICRC
			*/
			// payload copy
			int skip_after_ib = 0;
			copy_payload_to_new_memory(pkt, skip_after_ib, shm_p);

		}


	return portid;
}


/*
包解析处理主函数；
do steps3 传参为 0；即不进行下面的step3 相关操作
*/
static inline void
l3fwd_em_process_packets(int nb_rx, struct rte_mbuf **pkts_burst,
			 uint16_t *dst_port, uint16_t portid,
			 struct lcore_conf *qconf, const uint8_t do_step3, char *shm_p)
{
	int32_t i, j, pos;

	/*
	 * Send nb_rx - nb_rx % EM_HASH_LOOKUP_COUNT packets
	 * in groups of EM_HASH_LOOKUP_COUNT.
	 */
	int32_t n = RTE_ALIGN_FLOOR(nb_rx, EM_HASH_LOOKUP_COUNT);

	for (j = 0; j < EM_HASH_LOOKUP_COUNT && j < nb_rx; j++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j],
					       struct rte_ether_hdr *) + 1);
	}

	for (j = 0; j < n; j += EM_HASH_LOOKUP_COUNT) {

		uint32_t pkt_type = RTE_PTYPE_L3_MASK |
				    RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP;
		uint32_t l3_type, tcp_or_udp;

		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
			pkt_type &= pkts_burst[j + i]->packet_type;

		l3_type = pkt_type & RTE_PTYPE_L3_MASK;
		tcp_or_udp = pkt_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);

		for (i = 0, pos = j + EM_HASH_LOOKUP_COUNT;
		     i < EM_HASH_LOOKUP_COUNT && pos < nb_rx; i++, pos++) {
			rte_prefetch0(rte_pktmbuf_mtod(
					pkts_burst[pos],
					struct rte_ether_hdr *) + 1);
		}

		if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV4)) {

			em_get_dst_port_ipv4xN(qconf, &pkts_burst[j], portid,
					       &dst_port[j]);
			TOTAL_IPV4_NUM += EM_HASH_LOOKUP_COUNT;

		} else if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV6)) {

			em_get_dst_port_ipv6xN(qconf, &pkts_burst[j], portid,
					       &dst_port[j], shm_p);
			TOTAL_IPV6_NUM += EM_HASH_LOOKUP_COUNT;

		} else {
			for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
				dst_port[j + i] = em_get_dst_port(qconf,
						pkts_burst[j + i], portid, shm_p);
		}

		for (i = 0; i < EM_HASH_LOOKUP_COUNT && do_step3; i += FWDSTEP)
			processx4_step3(&pkts_burst[j + i], &dst_port[j + i]);
	}

	for (; j < nb_rx; j++) {
		dst_port[j] = em_get_dst_port(qconf, pkts_burst[j], portid, shm_p);
		if (do_step3)
			process_packet(pkts_burst[j], &pkts_burst[j]->port);
	}
}

/*
 * Buffer optimized handling of packets, invoked
 * from main_loop.
 */
static inline void
l3fwd_em_send_packets(int nb_rx, struct rte_mbuf **pkts_burst, uint16_t portid,
		      struct lcore_conf *qconf, char *shm_p)
{
	uint16_t dst_port[MAX_PKT_BURST];

	l3fwd_em_process_packets(nb_rx, pkts_burst, dst_port, portid, qconf, 0, shm_p);
	TOTAL_PACS += nb_rx;
	// send_packets_multi(qconf, pkts_burst, dst_port, nb_rx);
	RTE_LOG(INFO, L3FWD, "当前直接释放了 %d 个数据包; 目前已收集 %d 个 ipv6 数据包; 总共收集到 %d 个数据包\n", nb_rx, TOTAL_IPV6_NUM, TOTAL_PACS);
	for(int m = 0; m < nb_rx; m++)
		rte_pktmbuf_free(pkts_burst[m]);
}

/*
 * Buffer optimized handling of events, invoked
 * from main_loop.
 */
static inline void
l3fwd_em_process_events(int nb_rx, struct rte_event **ev,
		     struct lcore_conf *qconf)
{
	int32_t i, j, pos;
	char *shm_p;
	uint16_t dst_port[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	/*
	 * Send nb_rx - nb_rx % EM_HASH_LOOKUP_COUNT packets
	 * in groups of EM_HASH_LOOKUP_COUNT.
	 */
	int32_t n = RTE_ALIGN_FLOOR(nb_rx, EM_HASH_LOOKUP_COUNT);

	for (j = 0; j < nb_rx; j++)
		pkts_burst[j] = ev[j]->mbuf;

	for (j = 0; j < n; j += EM_HASH_LOOKUP_COUNT) {

		uint32_t pkt_type = RTE_PTYPE_L3_MASK |
				    RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP;
		uint32_t l3_type, tcp_or_udp;

		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
			pkt_type &= pkts_burst[j + i]->packet_type;

		l3_type = pkt_type & RTE_PTYPE_L3_MASK;
		tcp_or_udp = pkt_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);

		for (i = 0, pos = j + EM_HASH_LOOKUP_COUNT;
		     i < EM_HASH_LOOKUP_COUNT && pos < nb_rx; i++, pos++) {
			rte_prefetch0(rte_pktmbuf_mtod(
					pkts_burst[pos],
					struct rte_ether_hdr *) + 1);
		}

		if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV4)) {

			em_get_dst_port_ipv4xN_events(qconf, &pkts_burst[j],
					       &dst_port[j]);
			// TOTAL_IPV4_NUM += EM_HASH_LOOKUP_COUNT;

		} else if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV6)) {

			em_get_dst_port_ipv6xN_events(qconf, &pkts_burst[j],
					       &dst_port[j]);
			// TOTAL_IPV6_NUM += EM_HASH_LOOKUP_COUNT;

		} else {
			for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
				pkts_burst[j + i]->port = em_get_dst_port(qconf,
						pkts_burst[j + i],
						pkts_burst[j + i]->port, shm_p);
				process_packet(pkts_burst[j + i],
						&pkts_burst[j + i]->port);
			}
			continue;
		}
		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i += FWDSTEP)
			processx4_step3(&pkts_burst[j + i], &dst_port[j + i]);

		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
			pkts_burst[j + i]->port = dst_port[j + i];

	}

	for (; j < nb_rx; j++) {
		pkts_burst[j]->port = em_get_dst_port(qconf, pkts_burst[j],
						      pkts_burst[j]->port, shm_p);
		process_packet(pkts_burst[j], &pkts_burst[j]->port);
	}
}

static inline void
l3fwd_em_process_event_vector(struct rte_event_vector *vec,
			      struct lcore_conf *qconf, uint16_t *dst_port)
{
	uint16_t i;
	char *shm_p;
	if (vec->attr_valid)
		l3fwd_em_process_packets(vec->nb_elem, vec->mbufs, dst_port,
					 vec->port, qconf, 1, shm_p);
	else
		for (i = 0; i < vec->nb_elem; i++)
			l3fwd_em_process_packets(1, &vec->mbufs[i],
						 &dst_port[i],
						 vec->mbufs[i]->port, qconf, 1, shm_p);
	process_event_vector(vec, dst_port);
}

#endif /* __L3FWD_EM_HLM_H__ */
