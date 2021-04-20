/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef _RTE_GENEVE_H_
#define _RTE_GENEVE_H_

/**
 * @file
 *
 * GENEVE-related definitions
 */
#include <stdint.h>

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/** GENEVE default port. */
#define RTE_GENEVE_DEFAULT_PORT 6081

/**
 * GENEVE protocol header. (draft-ietf-nvo3-geneve-09)
 * Contains:
 * 2-bits version (must be 0).
 * 6-bits option length in four byte multiples, not including the eight
 *	bytes of the fixed tunnel header.
 * 1-bit control packet.
 * 1-bit critical options in packet.
 * 6-bits reserved
 * 16-bits Protocol Type. The protocol data unit after the Geneve header
 *	following the EtherType convention. Ethernet itself is represented by
 *	the value 0x6558.
 * 24-bits Virtual Network Identifier (VNI). Virtual network unique identified.
 * 8-bits reserved bits (must be 0 on transmission and ignored on receipt).
 * More-bits (optional) variable length options.
 */
__extension__
struct rte_geneve_hdr {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t ver:2;		/**< Version. */
	uint8_t opt_len:6;	/**< Options length. */
	uint8_t oam:1;		/**< Control packet. */
	uint8_t critical:1;	/**< Critical packet. */
	uint8_t reserved1:6;	/**< Reserved. */
#else
	uint8_t opt_len:6;	/**< Options length. */
	uint8_t ver:2;		/**< Version. */
	uint8_t reserved1:6;	/**< Reserved. */
	uint8_t critical:1;	/**< Critical packet. */
	uint8_t oam:1;		/**< Control packet. */
#endif
	rte_be16_t proto;	/**< Protocol type. */
	uint8_t vni[3];		/**< Virtual network identifier. */
	uint8_t reserved2;	/**< Reserved. */
	uint32_t opts[];	/**< Variable length options. */
} __rte_packed;

/* GENEVE ETH next protocol types */
#define RTE_GENEVE_TYPE_ETH	0x6558 /**< Ethernet Protocol. */

#ifdef __cplusplus
}
#endif

#endif /* RTE_GENEVE_H_ */
