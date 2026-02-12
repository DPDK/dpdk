/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 */

#ifndef _RTE_GRE_H_
#define _RTE_GRE_H_

#include <stdint.h>
#include <rte_byteorder.h>

/**
 * @file
 *
 * GRE headers definition.
 *
 * Generic Routing Encapsulation (GRE) is a tunneling protocol
 * that can encapsulate a wide variety of network layer protocols
 * inside virtual point-to-point links or point-to-multipoint links
 * over an Internet Protocol network.
 */

/**
 * GRE Header
 */
__extension__
struct __rte_packed_begin rte_gre_hdr {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint16_t res2:4; /**< Reserved */
	uint16_t s:1;    /**< Sequence Number Present bit */
	uint16_t k:1;    /**< Key Present bit */
	uint16_t res1:1; /**< Reserved */
	uint16_t c:1;    /**< Checksum Present bit */
	uint16_t ver:3;  /**< Version Number */
	uint16_t res3:5; /**< Reserved */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint16_t c:1;    /**< Checksum Present bit */
	uint16_t res1:1; /**< Reserved */
	uint16_t k:1;    /**< Key Present bit */
	uint16_t s:1;    /**< Sequence Number Present bit */
	uint16_t res2:4; /**< Reserved */
	uint16_t res3:5; /**< Reserved */
	uint16_t ver:3;  /**< Version Number */
#endif
	rte_be16_t proto;  /**< Protocol Type */
} __rte_packed_end;

/**
 * Optional field checksum in GRE header
 */
struct __rte_packed_begin rte_gre_hdr_opt_checksum_rsvd {
	rte_be16_t checksum;
	rte_be16_t reserved1;
} __rte_packed_end;

/**
 * Optional field key in GRE header
 */
struct __rte_packed_begin rte_gre_hdr_opt_key {
	rte_be32_t key;
} __rte_packed_end;

/**
 * Optional field sequence in GRE header
 */
struct __rte_packed_begin rte_gre_hdr_opt_sequence {
	rte_be32_t sequence;
} __rte_packed_end;

#endif /* RTE_GRE_H_ */
