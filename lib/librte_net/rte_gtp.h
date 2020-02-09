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
struct rte_gtp_hdr {
	uint8_t gtp_hdr_info; /**< GTP header info */
	uint8_t msg_type;     /**< GTP message type */
	uint16_t plen;        /**< Total payload length */
	uint32_t teid;        /**< Tunnel endpoint ID */
} __rte_packed;

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
