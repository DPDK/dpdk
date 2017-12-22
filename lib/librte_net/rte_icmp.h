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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ICMP Header
 */
struct icmp_hdr {
	uint8_t  icmp_type;   /* ICMP packet type. */
	uint8_t  icmp_code;   /* ICMP packet code. */
	uint16_t icmp_cksum;  /* ICMP packet checksum. */
	uint16_t icmp_ident;  /* ICMP packet identifier. */
	uint16_t icmp_seq_nb; /* ICMP packet sequence number. */
} __attribute__((__packed__));

/* ICMP packet types */
#define IP_ICMP_ECHO_REPLY   0
#define IP_ICMP_ECHO_REQUEST 8

#ifdef __cplusplus
}
#endif

#endif /* RTE_ICMP_H_ */
