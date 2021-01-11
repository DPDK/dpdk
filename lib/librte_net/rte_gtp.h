/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2010-2014 Intel Corporation.
 * All rights reserved.
 */

#ifndef _RTE_GTP_H_
#define _RTE_GTP_H_

/**
 * @file
 *
 * GTP-related defines
 */

#include <stdint.h>
#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simplified GTP protocol header.
 * Contains 8-bit header info, 8-bit message type,
 * 16-bit payload length after mandatory header, 32-bit TEID.
 * No optional fields and next extension header.
 */
__extension__
struct rte_gtp_hdr {
	union {
		uint8_t gtp_hdr_info; /**< GTP header info */
		struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
			uint8_t pn:1;   /**< N-PDU Number present bit */
			uint8_t s:1;    /**< Sequence Number Present bit */
			uint8_t e:1;    /**< Extension Present bit */
			uint8_t res1:1; /**< Reserved */
			uint8_t pt:1;   /**< Protocol Type bit */
			uint8_t ver:3;  /**< Version Number */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint8_t ver:3;  /**< Version Number */
			uint8_t pt:1;   /**< Protocol Type bit */
			uint8_t res1:1; /**< Reserved */
			uint8_t e:1;    /**< Extension Present bit */
			uint8_t s:1;    /**< Sequence Number Present bit */
			uint8_t pn:1;   /**< N-PDU Number present bit */
#endif
		};
	};
	uint8_t msg_type;     /**< GTP message type */
	rte_be16_t plen;      /**< Total payload length */
	rte_be32_t teid;      /**< Tunnel endpoint ID */
} __rte_packed;

/* Optional word of GTP header, present if any of E, S, PN is set. */
struct rte_gtp_hdr_ext_word {
	rte_be16_t sqn;	      /**< Sequence Number. */
	uint8_t npdu;	      /**< N-PDU number. */
	uint8_t next_ext;     /**< Next Extension Header Type. */
}  __rte_packed;

/** GTP header length */
#define RTE_ETHER_GTP_HLEN \
	(sizeof(struct rte_udp_hdr) + sizeof(struct rte_gtp_hdr))
/* GTP next protocal type */
#define RTE_GTP_TYPE_IPV4 0x40 /**< GTP next protocal type IPv4 */
#define RTE_GTP_TYPE_IPV6 0x60 /**< GTP next protocal type IPv6 */
/* GTP destination port number */
#define RTE_GTPC_UDP_PORT 2123 /**< GTP-C UDP destination port */
#define RTE_GTPU_UDP_PORT 2152 /**< GTP-U UDP destination port */

#ifdef __cplusplus
}
#endif

#endif /* RTE_GTP_H_ */
