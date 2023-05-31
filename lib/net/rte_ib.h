/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_IB_H
#define RTE_IB_H

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
struct rte_ib_bth {
	uint8_t	opcode;		/**< Opcode. */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t	tver:4;		/**< Transport Header Version. */
	uint8_t	padcnt:2;	/**< Pad Count. */
	uint8_t	m:1;		/**< MigReq. */
	uint8_t	se:1;		/**< Solicited Event. */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t	se:1;		/**< Solicited Event. */
	uint8_t	m:1;		/**< MigReq. */
	uint8_t	padcnt:2;	/**< Pad Count. */
	uint8_t	tver:4;		/**< Transport Header Version. */
#endif
	rte_be16_t pkey;	/**< Partition key. */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t	rsvd0:6;	/**< Reserved. */
	uint8_t	b:1;		/**< BECN. */
	uint8_t	f:1;		/**< FECN. */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t	f:1;		/**< FECN. */
	uint8_t	b:1;		/**< BECN. */
	uint8_t	rsvd0:6;	/**< Reserved. */
#endif
	uint8_t	dst_qp[3];	/**< Destination QP */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t	rsvd1:7;	/**< Reserved. */
	uint8_t	a:1;		/**< Acknowledge Request. */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t	a:1;		/**< Acknowledge Request. */
	uint8_t	rsvd1:7;	/**< Reserved. */
#endif
	uint8_t	psn[3];		/**< Packet Sequence Number */
} __rte_packed;

/** RoCEv2 default port. */
#define RTE_ROCEV2_DEFAULT_PORT 4791

#ifdef __cplusplus
}
#endif

#endif /* RTE_IB_H */
