/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef L3FWD_ACL_H
#define L3FWD_ACL_H

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
#define L3FWDACL_DEBUG
#endif

#define MAX_ACL_RULE_NUM	100000
#define DEFAULT_MAX_CATEGORIES	1
#define L3FWD_ACL_IPV4_NAME	"l3fwd-acl-ipv4"
#define L3FWD_ACL_IPV6_NAME	"l3fwd-acl-ipv6"

#define ACL_DENY_SIGNATURE	0xf0000000
#define RTE_LOGTYPE_L3FWDACL	RTE_LOGTYPE_USER3
#define acl_log(format, ...)	RTE_LOG(ERR, L3FWDACL, format, ##__VA_ARGS__)
#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (unsigned char)(ip >> 24 & 0xff);\
		*b = (unsigned char)(ip >> 16 & 0xff);\
		*c = (unsigned char)(ip >> 8 & 0xff);\
		*d = (unsigned char)(ip & 0xff);\
	} while (0)
#define OFF_ETHHEAD	(sizeof(struct rte_ether_hdr))
#define OFF_IPV42PROTO (offsetof(struct rte_ipv4_hdr, next_proto_id))
#define OFF_IPV62PROTO (offsetof(struct rte_ipv6_hdr, proto))
#define MBUF_IPV4_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, OFF_ETHHEAD + OFF_IPV42PROTO)
#define MBUF_IPV6_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, OFF_ETHHEAD + OFF_IPV62PROTO)

/*
 * ACL rules should have higher priorities than route ones to ensure ACL rule
 * always be found when input packets have multi-matches in the database.
 * A exception case is performance measure, which can define route rules with
 * higher priority and route rules will always be returned in each lookup.
 * Reserve range from ACL_RULE_PRIORITY_MAX + 1 to
 * RTE_ACL_MAX_PRIORITY for route entries in performance measure
 */
#define ACL_RULE_PRIORITY_MAX 0x10000000

/*
 * Forward port info save in ACL lib starts from 1
 * since ACL assume 0 is invalid.
 * So, need add 1 when saving and minus 1 when forwarding packets.
 */
#define FWD_PORT_SHIFT 1

void
print_one_ipv4_rule(struct acl4_rule *rule, int extra);

void
print_one_ipv6_rule(struct acl6_rule *rule, int extra);

#endif /* L3FWD_ACL_H */
