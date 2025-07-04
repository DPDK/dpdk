/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 6WIND S.A.
 */

#ifndef _RTE_MPLS_H_
#define _RTE_MPLS_H_

/**
 * @file
 *
 * MPLS-related defines
 */

#include <stdint.h>
#include <rte_byteorder.h>

/**
 * MPLS header.
 */
__extension__
struct __rte_packed_begin rte_mpls_hdr {
	rte_be16_t tag_msb; /**< Label(msb). */
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t tag_lsb:4;  /**< Label(lsb). */
	uint8_t tc:3;       /**< Traffic class. */
	uint8_t bs:1;       /**< Bottom of stack. */
#else
	uint8_t bs:1;       /**< Bottom of stack. */
	uint8_t tc:3;       /**< Traffic class. */
	uint8_t tag_lsb:4;  /**< label(lsb) */
#endif
	uint8_t  ttl;       /**< Time to live. */
} __rte_packed_end;

#endif /* RTE_MPLS_H_ */
