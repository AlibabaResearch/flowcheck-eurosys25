/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_IMMDT_H
#define RTE_IMMDT_H

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
struct rte_immdt_bth {
	uint32_t immdt_data; /* Immediate Data*/
} __rte_packed;

/** RoCEv2 default port. */
#define RTE_ROCEV2_DEFAULT_PORT 4791

#ifdef __cplusplus
}
#endif

#endif /* RTE_IMMDT_H */
