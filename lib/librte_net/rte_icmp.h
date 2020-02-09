/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2013 6WIND S.A.
 * All rights reserved.
 */

#ifndef _RTE_ICMP_H_
#define _RTE_ICMP_H_

/**
 * @file
 *
 * ICMP-related defines
 */

#include <stdint.h>

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ICMP Header
 */
struct rte_icmp_hdr {
	uint8_t  icmp_type;     /* ICMP packet type. */
	uint8_t  icmp_code;     /* ICMP packet code. */
	rte_be16_t icmp_cksum;  /* ICMP packet checksum. */
	rte_be16_t icmp_ident;  /* ICMP packet identifier. */
	rte_be16_t icmp_seq_nb; /* ICMP packet sequence number. */
} __rte_packed;

/* ICMP packet types */
#define RTE_IP_ICMP_ECHO_REPLY   0
#define RTE_IP_ICMP_ECHO_REQUEST 8

#ifdef __cplusplus
}
#endif

#endif /* RTE_ICMP_H_ */
