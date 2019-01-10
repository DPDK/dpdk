/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _IPH_H_
#define _IPH_H_

/**
 * @file iph.h
 * Contains functions/structures/macros to manipulate IPv4/IPv6 headers
 * used internally by ipsec library.
 */

/*
 * Move preceding (L3) headers down to remove ESP header and IV.
 */
static inline void
remove_esph(char *np, char *op, uint32_t hlen)
{
	uint32_t i;

	for (i = hlen; i-- != 0; np[i] = op[i])
		;
}

/*
 * Move preceding (L3) headers up to free space for ESP header and IV.
 */
static inline void
insert_esph(char *np, char *op, uint32_t hlen)
{
	uint32_t i;

	for (i = 0; i != hlen; i++)
		np[i] = op[i];
}

/* update original ip header fields for transport case */
static inline int
update_trs_l3hdr(const struct rte_ipsec_sa *sa, void *p, uint32_t plen,
		uint32_t l2len, uint32_t l3len, uint8_t proto)
{
	struct ipv4_hdr *v4h;
	struct ipv6_hdr *v6h;
	int32_t rc;

	if ((sa->type & RTE_IPSEC_SATP_IPV_MASK) == RTE_IPSEC_SATP_IPV4) {
		v4h = p;
		rc = v4h->next_proto_id;
		v4h->next_proto_id = proto;
		v4h->total_length = rte_cpu_to_be_16(plen - l2len);
	} else if (l3len == sizeof(*v6h)) {
		v6h = p;
		rc = v6h->proto;
		v6h->proto = proto;
		v6h->payload_len = rte_cpu_to_be_16(plen - l2len -
				sizeof(*v6h));
	/* need to add support for IPv6 with options */
	} else
		rc = -ENOTSUP;

	return rc;
}

/* update original and new ip header fields for tunnel case */
static inline void
update_tun_l3hdr(const struct rte_ipsec_sa *sa, void *p, uint32_t plen,
		uint32_t l2len, rte_be16_t pid)
{
	struct ipv4_hdr *v4h;
	struct ipv6_hdr *v6h;

	if (sa->type & RTE_IPSEC_SATP_MODE_TUNLV4) {
		v4h = p;
		v4h->packet_id = pid;
		v4h->total_length = rte_cpu_to_be_16(plen - l2len);
	} else {
		v6h = p;
		v6h->payload_len = rte_cpu_to_be_16(plen - l2len -
				sizeof(*v6h));
	}
}

#endif /* _IPH_H_ */
