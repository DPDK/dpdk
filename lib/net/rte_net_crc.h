/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef _RTE_NET_CRC_H_
#define _RTE_NET_CRC_H_

#include <stdint.h>
#include <rte_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** CRC types */
enum rte_net_crc_type {
	RTE_NET_CRC16_CCITT = 0,
	RTE_NET_CRC32_ETH,
	RTE_NET_CRC_REQS
};

/** CRC compute algorithm */
enum rte_net_crc_alg {
	RTE_NET_CRC_SCALAR = 0,
	RTE_NET_CRC_SSE42,
	RTE_NET_CRC_NEON,
	RTE_NET_CRC_AVX512,
};

/** CRC context (algorithm, type) */
struct rte_net_crc;

/**
 * Frees the memory space pointed to by the CRC context pointer.
 * If the pointer is NULL, the function does nothing.
 *
 * @param ctx
 *   Pointer to the CRC context
 */
void
rte_net_crc_free(struct rte_net_crc *crc);

/**
 * Set the CRC context (i.e. scalar version,
 * x86 64-bit sse4.2 intrinsic version, etc.) and internal data
 * structure.
 *
 * @param alg
 *   This parameter is used to select the CRC implementation version.
 *   - RTE_NET_CRC_SCALAR
 *   - RTE_NET_CRC_SSE42 (Use 64-bit SSE4.2 intrinsic)
 *   - RTE_NET_CRC_NEON (Use ARM Neon intrinsic)
 *   - RTE_NET_CRC_AVX512 (Use 512-bit AVX intrinsic)
 * @param type
 *   CRC type (enum rte_net_crc_type)
 *
 * @return
 *   Pointer to the CRC context
 */
struct rte_net_crc *
rte_net_crc_set_alg(enum rte_net_crc_alg alg, enum rte_net_crc_type type)
	__rte_malloc __rte_dealloc(rte_net_crc_free, 1);

/**
 * CRC compute API
 *
 * Note:
 * The command line argument --force-max-simd-bitwidth will be ignored
 * by processes that have not created this CRC context.
 *
 * @param ctx
 *   Pointer to the CRC context
 * @param data
 *   Pointer to the packet data for CRC computation
 * @param data_len
 *   Data length for CRC computation
 *
 * @return
 *   CRC value
 */
uint32_t
rte_net_crc_calc(const struct rte_net_crc *ctx,
	const void *data, const uint32_t data_len);

#ifdef __cplusplus
}
#endif


#endif /* _RTE_NET_CRC_H_ */
