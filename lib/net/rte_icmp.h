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

/**
 * ICMP base header
 */
struct __rte_packed_begin rte_icmp_base_hdr {
	uint8_t type;
	uint8_t code;
	rte_be16_t checksum;
} __rte_packed_end;

/**
 * ICMP echo header
 */
struct __rte_packed_begin rte_icmp_echo_hdr {
	struct rte_icmp_base_hdr base;
	rte_be16_t identifier;
	rte_be16_t sequence;
} __rte_packed_end;

/**
 * ICMP Header
 *
 * @see rte_icmp_echo_hdr which is similar.
 */
struct __rte_packed_begin rte_icmp_hdr {
	uint8_t  icmp_type;     /* ICMP packet type. */
	uint8_t  icmp_code;     /* ICMP packet code. */
	rte_be16_t icmp_cksum;  /* ICMP packet checksum. */
	rte_be16_t icmp_ident;  /* ICMP packet identifier. */
	rte_be16_t icmp_seq_nb; /* ICMP packet sequence number. */
} __rte_packed_end;

/* ICMP packet types */
#define RTE_ICMP_TYPE_ECHO_REPLY 0
#define RTE_IP_ICMP_ECHO_REPLY \
	(RTE_DEPRECATED(RTE_IP_ICMP_ECHO_REPLY) RTE_ICMP_TYPE_ECHO_REPLY)
#define RTE_ICMP_TYPE_DEST_UNREACHABLE 3
#define RTE_ICMP_TYPE_REDIRECT 5
#define RTE_ICMP_TYPE_ECHO_REQUEST 8
#define RTE_IP_ICMP_ECHO_REQUEST \
	(RTE_DEPRECATED(RTE_IP_ICMP_ECHO_REQUEST) RTE_ICMP_TYPE_ECHO_REQUEST)
#define RTE_ICMP_TYPE_TTL_EXCEEDED 11
#define RTE_ICMP_TYPE_PARAM_PROBLEM 12
#define RTE_ICMP_TYPE_TIMESTAMP_REQUEST 13
#define RTE_ICMP_TYPE_TIMESTAMP_REPLY 14

/* Destination Unreachable codes */
#define RTE_ICMP_CODE_UNREACH_NET 0
#define RTE_ICMP_CODE_UNREACH_HOST 1
#define RTE_ICMP_CODE_UNREACH_PROTO 2
#define RTE_ICMP_CODE_UNREACH_PORT 3
#define RTE_ICMP_CODE_UNREACH_FRAG 4
#define RTE_ICMP_CODE_UNREACH_SRC 5

/* Time Exceeded codes */
#define RTE_ICMP_CODE_TTL_EXCEEDED 0
#define RTE_ICMP_CODE_TTL_FRAG 1

/* Redirect codes */
#define RTE_ICMP_CODE_REDIRECT_NET 0
#define RTE_ICMP_CODE_REDIRECT_HOST 1
#define RTE_ICMP_CODE_REDIRECT_TOS_NET 2
#define RTE_ICMP_CODE_REDIRECT_TOS_HOST 3

#define RTE_ICMP6_ECHO_REQUEST 128
#define RTE_ICMP6_ECHO_REPLY   129

#endif /* RTE_ICMP_H_ */
