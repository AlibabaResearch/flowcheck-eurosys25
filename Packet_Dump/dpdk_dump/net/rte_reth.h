/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_RETH_H
#define RTE_RETH_H

/**
 * @file
 *
 * InfiniBand headers definitions
 *
 * The infiniBand headers are used by RoCE (RDMA over Converged Ethernet).
 */

#include <stdint.h>

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * InfiniBand Base Transport Header according to
 * IB Specification Vol 1-Release-1.4.
 */
__extension__
struct rte_reth_bth {
    uint8_t virtual_address[8]; /* Virtual addr*/
    uint8_t remote_key[4]; /* Remote Key*/
    uint8_t DMA_length[4]; /* DMA length*/
    // uint64_t virtual_address; 
    // uint32_t remote_key;
    // uint32_t DMA_length;
} __rte_packed;

/** RoCEv2 default port. */
#define RTE_ROCEV2_DEFAULT_PORT 4791
    
#ifdef __cplusplus
}
#endif

#endif /* RTE_RETH_H */
